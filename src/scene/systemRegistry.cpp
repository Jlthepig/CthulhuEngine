#include "systemRegistry.h"
#include "engine.h"
#include "components.h"
#include "audio.h"
#include "physics.h"
#include "gtc/matrix_transform.hpp"
#include "log_utils.hpp"

namespace Cthulhu::Scene
{
    void RegisterCoreSystems(flecs::world& world, Cthulhu::Engine* engineContext)
    {
        // register physics syncing system
        world.system<TransformComponent, const PhysicsComponent>("PhysicsSyncSystem")
            .each([engineContext](TransformComponent& transform, const PhysicsComponent& phys)
            {
                if (phys.hasBody)
                {
                    auto bodyTransform = engineContext->getPhysicsWorld().getBodyTransform(phys.bodyId);
                    transform.position = bodyTransform.position;
                    transform.rotation = bodyTransform.rotation;
                    transform.matrixDirty = true;
                }
            }
        );

        // register transform system
        world.system<TransformComponent>("TransformSystem")
            .each([](flecs::entity e, TransformComponent& transform)
            {
                glm::mat4 localMatrix = glm::mat4(1.0f);
                localMatrix = glm::translate(localMatrix, transform.position);
                localMatrix = glm::rotate(localMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                localMatrix = glm::rotate(localMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                localMatrix = glm::rotate(localMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                localMatrix = glm::scale(localMatrix, transform.scale);

                // combine with parent matrix if it has a parent
                glm::mat4 globalMatrix = localMatrix;
                auto parent = e.parent();
                if (parent.is_alive() && parent.has<TransformComponent>())
                {
                    const auto& parentTransform = parent.get<TransformComponent>();
                    globalMatrix = parentTransform.cachedModelMatrix * localMatrix;
                }
                transform.cachedModelMatrix = globalMatrix;
                transform.cachedNormalMatrix = glm::transpose(glm::inverse(globalMatrix));
            });

        world.system<CharacterControllerComponent, TransformComponent>("CharacterInterpolationSystem")
            .each([engineContext]([[maybe_unused]] flecs::entity e, CharacterControllerComponent& cc, TransformComponent& transform) 
            {
                float alpha = engineContext->getPhysicsWorld().getInterpolationAlpha();
                transform.position = glm::mix(cc.prevPos, cc.currentPos, alpha);
                transform.matrixDirty = true;
            });

        world.system<WeaponComponent, const TransformComponent,const CameraComponent>("WeaponSystem")
            .each([engineContext]([[maybe_unused]] flecs::entity e, WeaponComponent& wep, const TransformComponent& trans, const CameraComponent& cam)
            {
                // cooldown
                wep.timeSinceLastShot += engineContext->getDeltaTime();

                if (wep.wantsToFire)
                {
                    float cooldown = 1.0 / wep.firerate;

                    if (wep.timeSinceLastShot >= cooldown)
                    {
                       glm::vec3 origin = glm::vec3(trans.cachedModelMatrix[3]);
                        glm::vec3 forward = glm::normalize(cam.front);
                        forward = glm::normalize(forward);

                        Physics::RaycastHitInfo hit = engineContext->getPhysicsWorld().raycast(origin, forward, wep.maxRange);

                        if (!hit.didHit)
                        {
                            hit.position = origin + (forward * wep.maxRange);
                            hit.distance = wep.maxRange;
                        }

                        engineContext->triggerRaycastCallback(hit);
                        wep.timeSinceLastShot = 0.0f;
                    }
                    wep.wantsToFire = false;
                }
            });

            world.system<AudioSourceComponent>("AudioSystem")
                .each([]([[maybe_unused]] flecs::entity e, AudioSourceComponent& audio)
                {
                    if (audio.playTrigger)
                    {
                        if (audio.isPlaying && audio.soundInstanceId != 0)
                        {
                            Core::Audio::stopSound(audio.soundInstanceId);
                        }
                        audio.soundInstanceId = Core::Audio::playSound2D(audio.filePath, audio.volume, audio.loop);
                        audio.isPlaying = (audio.soundInstanceId != 0);
                        audio.playTrigger = false;
                    }
                    if (audio.stopTrigger && audio.isPlaying)
                    {
                        Core::Audio::stopSound(audio.soundInstanceId);
                        audio.isPlaying = false;
                        audio.soundInstanceId = 0;
                        audio.stopTrigger = false;
                    }
                });

                // safety check if audio entity is destroyed, stop the sound
                world.observer<AudioSourceComponent>("AudioCleanupObserver")
                    .event(flecs::OnRemove)
                    .each([]([[maybe_unused]] flecs::entity e, AudioSourceComponent& audio)
                    {
                        if (audio.isPlaying && audio.soundInstanceId != 0)
                        {
                            Core::Audio::stopSound(audio.soundInstanceId);
                        }
                    });
    }
}