#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <gtc/matrix_transform.hpp>

#include "audio.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "physics.hpp"
#include "systemRegistry.hpp"

namespace Cthulhu::Scene
{

namespace
{
    static bool createPhysicsRuntime(flecs::entity entity, Cthulhu::Engine *engine)
    {
        if (!entity.has<PhysicsComponent>() || !entity.has<TransformComponent>())
        {
            return false;
        }

        if (entity.has<PhysicsRuntimeComponent>())
        {
            return true;
        }

        const auto &physics = entity.get<PhysicsComponent>();
        const auto &transform = entity.get<TransformComponent>();

        uint32_t bodyId = 0;

        switch (physics.type)
        {
            case PhysicsBodyType::Static:
            {
                bodyId = engine->getPhysicsWorld().addStaticBox(transform.position, physics.halfExtent);
                break;
            }

            case PhysicsBodyType::Dynamic:
            {
                bodyId = engine->getPhysicsWorld().addDynamicBox(transform.position, physics.halfExtent, physics.mass);
                break;
            }
        }

        if (bodyId == 0)
        {
            return false;
        }

        entity.set(PhysicsRuntimeComponent{.bodyId = bodyId});
        return true;
    }
} // namespace

void RegisterCoreSystems(flecs::world &world, Cthulhu::Engine *engineContext)
{
    // << physics >>
    world.observer<PhysicsComponent>("PhysicsCreateObserver")
        .event(flecs::OnSet)
        .each([engineContext](flecs::entity entity, PhysicsComponent &)
        {
            if (entity.has<PhysicsRuntimeComponent>())
            {
                entity.remove<PhysicsRuntimeComponent>();
            }

            createPhysicsRuntime(entity, engineContext);
        });

    world.system<TransformComponent, const PhysicsRuntimeComponent>("PhysicsSyncSystem")
        .each([engineContext](TransformComponent &transform, const PhysicsRuntimeComponent &runtime)
        {
            if (runtime.bodyId == 0)
            {
                return;
            }

            const auto bodyTransform = engineContext->getPhysicsWorld().getBodyTransform(runtime.bodyId);

            transform.position = bodyTransform.position;
            transform.rotation = bodyTransform.rotation;
            transform.matrixDirty = true;
        });

    // << character >>
    world.system<CharacterControllerRuntimeComponent, TransformComponent>("CharacterInterpolationSystem")
        .each([engineContext](CharacterControllerRuntimeComponent &runtime, TransformComponent &transform)
        {
            const float alpha = engineContext->getPhysicsWorld().getInterpolationAlpha();

            transform.position = glm::mix(runtime.prevPos, runtime.currentPos, alpha);
            transform.matrixDirty = true;
        });

    // << transform >>
    world.system<TransformComponent, const TransformComponent *>("TransformSystem")
        .term_at(1)
        .parent()
        .cascade()
        .each([](TransformComponent &transform, const TransformComponent *parentTransform)
        {
            glm::mat4 localMatrix = glm::mat4(1.0f);
            localMatrix = glm::translate(localMatrix, transform.position);
            localMatrix = glm::rotate(localMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            localMatrix = glm::rotate(localMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            localMatrix = glm::rotate(localMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            localMatrix = glm::scale(localMatrix, transform.scale);

            if (parentTransform)
            {
                transform.cachedModelMatrix = parentTransform->cachedModelMatrix * localMatrix;
            }
            else
            {
                transform.cachedModelMatrix = localMatrix;
            }

            transform.cachedNormalMatrix = glm::transpose(glm::inverse(transform.cachedModelMatrix));
        });

    // << weapon >>
    world.observer<WeaponComponent>("WeaponRuntimeCreateObserver")
        .event(flecs::OnSet)
        .each([](flecs::entity entity, WeaponComponent &)
        {
            if (!entity.has<WeaponRuntimeComponent>())
            {
                entity.set(WeaponRuntimeComponent{});
            }
        });

    world.system<const WeaponComponent, WeaponRuntimeComponent, const TransformComponent, const CameraComponent>("WeaponSystem")
        .each([engineContext](const WeaponComponent &weapon,
                              WeaponRuntimeComponent &runtime,
                              const TransformComponent &transform,
                              const CameraComponent &camera)
        {
            runtime.timeSinceLastShot += engineContext->getDeltaTime();

            if (!runtime.wantsToFire)
            {
                return;
            }

            if (weapon.fireRate <= 0.0f)
            {
                runtime.wantsToFire = false;
                return;
            }

            const float cooldown = 1.0f / weapon.fireRate;

            if (runtime.timeSinceLastShot >= cooldown)
            {
                const glm::vec3 origin = glm::vec3(transform.cachedModelMatrix[3]);
                const glm::vec3 forward = glm::normalize(camera.front);

                Physics::RaycastHitInfo hit = engineContext->getPhysicsWorld().raycast(origin, forward, weapon.maxRange);

                if (!hit.didHit)
                {
                    hit.position = origin + forward * weapon.maxRange;
                    hit.distance = weapon.maxRange;
                }

                engineContext->triggerRaycastCallback(hit);

                runtime.timeSinceLastShot = 0.0f;
            }

            runtime.wantsToFire = false;
        });

    // << audio >>
    world.observer<AudioSourceComponent>("AudioRuntimeCreateObserver")
        .event(flecs::OnSet)
        .each([](flecs::entity entity, AudioSourceComponent &)
        {
            if (!entity.has<AudioSourceRuntimeComponent>())
            {
                entity.set(AudioSourceRuntimeComponent{});
            }
        });

    world.system<const AudioSourceComponent, AudioSourceRuntimeComponent>("AudioSystem")
        .each([engineContext](const AudioSourceComponent &source, AudioSourceRuntimeComponent &runtime)
        {
            if (runtime.playRequested)
            {
                if (runtime.isPlaying && runtime.soundInstanceId != 0)
                {
                    Core::Audio::stopSound(runtime.soundInstanceId);

                    runtime.isPlaying = false;
                    runtime.soundInstanceId = 0;
                }

                const auto *activeProject = engineContext->getProject();

                if (!activeProject)
                {
                    runtime.playRequested = false;
                    return;
                }

                auto resolvedAudioPath = activeProject->resolveResourcePath(source.filePath);

                if (!resolvedAudioPath)
                {
                    runtime.playRequested = false;
                    return;
                }

                runtime.soundInstanceId = Core::Audio::playSound2D(resolvedAudioPath->string(), source.volume, source.loop);

                runtime.isPlaying = runtime.soundInstanceId != 0;
                runtime.playRequested = false;
            }

            if (runtime.stopRequested)
            {
                if (runtime.isPlaying && runtime.soundInstanceId != 0)
                {
                    Core::Audio::stopSound(runtime.soundInstanceId);
                }

                runtime.isPlaying = false;
                runtime.soundInstanceId = 0;
                runtime.stopRequested = false;
            }
        });

    // << lifecycle removal observers >>
    world.observer<PhysicsComponent>("PhysicsRuntimeRemoveObserver")
        .event(flecs::OnRemove)
        .each([](flecs::entity entity, PhysicsComponent &)
        {
            entity.remove<PhysicsRuntimeComponent>();
        });

    world.observer<WeaponComponent>("WeaponRuntimeRemoveObserver")
        .event(flecs::OnRemove)
        .each([](flecs::entity entity, WeaponComponent &)
        {
            entity.remove<WeaponRuntimeComponent>();
        });

    world.observer<AudioSourceComponent>("AudioRuntimeRemoveObserver")
        .event(flecs::OnRemove)
        .each([](flecs::entity entity, AudioSourceComponent &)
        {
            entity.remove<AudioSourceRuntimeComponent>();
        });

    world.observer<CharacterControllerComponent>("CharacterControllerRuntimeRemoveObserver")
        .event(flecs::OnRemove)
        .each([](flecs::entity entity, CharacterControllerComponent &)
        {
            entity.remove<CharacterControllerRuntimeComponent>();
        });

    // << cleanup observers >>
    world.observer<PhysicsRuntimeComponent>("PhysicsCleanupObserver")
        .event(flecs::OnRemove)
        .each([engineContext](flecs::entity, PhysicsRuntimeComponent &runtime)
        {
            if (runtime.bodyId != 0)
            {
                engineContext->getPhysicsWorld().removeBody(runtime.bodyId);
            }
        });

    world.observer<CharacterControllerRuntimeComponent>("CharacterControllerCleanupObserver")
        .event(flecs::OnRemove)
        .each([](flecs::entity, CharacterControllerRuntimeComponent &runtime)
        {
            delete runtime.character;
            runtime.character = nullptr;
        });

    world.observer<AudioSourceRuntimeComponent>("AudioCleanupObserver")
        .event(flecs::OnRemove)
        .each([](flecs::entity, AudioSourceRuntimeComponent &runtime)
        {
            if (runtime.isPlaying && runtime.soundInstanceId != 0)
            {
                Core::Audio::stopSound(runtime.soundInstanceId);
            }
        });
}

} // namespace Cthulhu::Scene