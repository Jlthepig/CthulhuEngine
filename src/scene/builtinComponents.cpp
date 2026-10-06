#include "builtinComponents.hpp"

#include <limits>

#include "componentRegistry.hpp"
#include "components.hpp"

namespace Cthulhu::Scene
{

namespace
{

constexpr float unbounded = std::numeric_limits<float>::max();

ComponentDescriptor describeTransform()
{
    auto d = makeComponentDescriptor<TransformComponent>("Transform");
    d.core = true;

    d.fields.push_back(makeField(
        "position", FieldType::Vec3, [](flecs::entity e) -> FieldValue { return e.get<TransformComponent>().position; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<TransformComponent>();
            c.position = std::get<glm::vec3>(v);
            c.matrixDirty = true;
            e.set(c);
        },
        0.0f, 0.0f, 0.1f));

    auto rotation = makeField(
        "rotation", FieldType::Vec3, [](flecs::entity e) -> FieldValue { return e.get<TransformComponent>().rotation; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<TransformComponent>();
            c.rotation = std::get<glm::vec3>(v);
            c.matrixDirty = true;
            e.set(c);
        });
    rotation.hint = FieldHint::Degrees;
    d.fields.push_back(std::move(rotation));

    d.fields.push_back(makeField(
        "scale", FieldType::Vec3, [](flecs::entity e) -> FieldValue { return e.get<TransformComponent>().scale; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<TransformComponent>();
            c.scale = std::get<glm::vec3>(v);
            c.matrixDirty = true;
            e.set(c);
        },
        0.0f, 0.0f, 0.1f));

    return d;
}

ComponentDescriptor describeMesh()
{
    auto d = makeComponentDescriptor<MeshComponent>("Mesh");

    auto model = makeField(
        "modelPath", FieldType::AssetRef,
        [](flecs::entity e) -> FieldValue { return e.get<MeshComponent>().modelPath; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<MeshComponent>();
            c.modelPath = std::get<std::string>(v);
            e.set(c);
        });
    model.assetType = Assets::AssetType::Model;
    d.fields.push_back(std::move(model));

    return d;
}

ComponentDescriptor describePhysics()
{
    auto d = makeComponentDescriptor<PhysicsComponent>("Physics");

    auto type = makeField(
        "type", FieldType::Enum,
        [](flecs::entity e) -> FieldValue { return static_cast<int>(e.get<PhysicsComponent>().type); },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<PhysicsComponent>();
            c.type = static_cast<PhysicsBodyType>(std::get<int>(v));
            e.set(c);
        });
    type.enumNames = {"Static", "Dynamic"}; // HAS TO match PhysicsBodyType order >> IMPORTANT
    d.fields.push_back(std::move(type));

    d.fields.push_back(makeField(
        "halfExtent", FieldType::Vec3,
        [](flecs::entity e) -> FieldValue { return e.get<PhysicsComponent>().halfExtent; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<PhysicsComponent>();
            c.halfExtent = std::get<glm::vec3>(v);
            e.set(c);
        },
        0.0f, 0.0f, 0.05f));

    d.fields.push_back(makeField(
        "mass", FieldType::Float, [](flecs::entity e) -> FieldValue { return e.get<PhysicsComponent>().mass; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<PhysicsComponent>();
            c.mass = std::get<float>(v);
            e.set(c);
        },
        0.001f, unbounded, 0.1f));

    return d;
}

ComponentDescriptor describeWeapon()
{
    auto d = makeComponentDescriptor<WeaponComponent>("Weapon");

    d.fields.push_back(makeField(
        "fireRate", FieldType::Float, [](flecs::entity e) -> FieldValue { return e.get<WeaponComponent>().fireRate; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<WeaponComponent>();
            c.fireRate = std::get<float>(v);
            e.set(c);
        },
        0.0f, unbounded, 0.1f));

    d.fields.push_back(makeField(
        "maxRange", FieldType::Float, [](flecs::entity e) -> FieldValue { return e.get<WeaponComponent>().maxRange; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<WeaponComponent>();
            c.maxRange = std::get<float>(v);
            e.set(c);
        },
        0.0f, unbounded, 1.0f));

    return d;
}

ComponentDescriptor describeAudioSource()
{
    auto d = makeComponentDescriptor<AudioSourceComponent>("AudioSource");

    auto file = makeField(
        "filePath", FieldType::AssetRef,
        [](flecs::entity e) -> FieldValue { return e.get<AudioSourceComponent>().filePath; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<AudioSourceComponent>();
            c.filePath = std::get<std::string>(v);
            e.set(c);
        });
    file.assetType = Assets::AssetType::Audio;
    d.fields.push_back(std::move(file));

    d.fields.push_back(makeField(
        "volume", FieldType::Float, [](flecs::entity e) -> FieldValue { return e.get<AudioSourceComponent>().volume; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<AudioSourceComponent>();
            c.volume = std::get<float>(v);
            e.set(c);
        },
        0.0f, unbounded, 0.05f));

    d.fields.push_back(makeField(
        "loop", FieldType::Bool, [](flecs::entity e) -> FieldValue { return e.get<AudioSourceComponent>().loop; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<AudioSourceComponent>();
            c.loop = std::get<bool>(v);
            e.set(c);
        }));

    return d;
}

ComponentDescriptor describeCharacterController()
{
    auto d = makeComponentDescriptor<CharacterControllerComponent>("CharacterController");

    d.fields.push_back(makeField(
        "gravity", FieldType::Float,
        [](flecs::entity e) -> FieldValue { return e.get<CharacterControllerComponent>().gravity; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CharacterControllerComponent>();
            c.gravity = std::get<float>(v);
            e.set(c);
        },
        0.0f, 0.0f, 0.1f));

    d.fields.push_back(makeField(
        "jumpVelocity", FieldType::Float,
        [](flecs::entity e) -> FieldValue { return e.get<CharacterControllerComponent>().jumpVelocity; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CharacterControllerComponent>();
            c.jumpVelocity = std::get<float>(v);
            e.set(c);
        },
        0.0f, unbounded, 0.1f));

    d.fields.push_back(makeField(
        "capsuleRadius", FieldType::Float,
        [](flecs::entity e) -> FieldValue { return e.get<CharacterControllerComponent>().capsuleRadius; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CharacterControllerComponent>();
            c.capsuleRadius = std::get<float>(v);
            e.set(c);
        },
        0.01f, unbounded, 0.01f));

    d.fields.push_back(makeField(
        "capsuleHeight", FieldType::Float,
        [](flecs::entity e) -> FieldValue { return e.get<CharacterControllerComponent>().capsuleHeight; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CharacterControllerComponent>();
            c.capsuleHeight = std::get<float>(v);
            e.set(c);
        },
        0.01f, unbounded, 0.05f));

    d.fields.push_back(makeField(
        "maxWalkableSlope", FieldType::Float,
        [](flecs::entity e) -> FieldValue { return e.get<CharacterControllerComponent>().maxWalkableSlope; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CharacterControllerComponent>();
            c.maxWalkableSlope = std::get<float>(v);
            e.set(c);
        },
        0.0f, 90.0f, 1.0f));

    d.fields.push_back(makeField(
        "maxPushStrength", FieldType::Float,
        [](flecs::entity e) -> FieldValue { return e.get<CharacterControllerComponent>().maxPushStrength; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CharacterControllerComponent>();
            c.maxPushStrength = std::get<float>(v);
            e.set(c);
        },
        0.0f, unbounded, 1.0f));

    return d;
}

ComponentDescriptor describeCamera()
{
    auto d = makeComponentDescriptor<CameraComponent>("Camera");

    d.fields.push_back(makeField(
        "front", FieldType::Vec3, [](flecs::entity e) -> FieldValue { return e.get<CameraComponent>().front; },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<CameraComponent>();
            c.front = std::get<glm::vec3>(v);
            e.set(c);
        }));

    return d;
}

} // namespace

bool registerBuiltinComponents(ComponentRegistry &registry)
{
    return registry.registerComponent(describeTransform()) && registry.registerComponent(describeMesh()) &&
           registry.registerComponent(describePhysics()) && registry.registerComponent(describeWeapon()) &&
           registry.registerComponent(describeAudioSource()) &&
           registry.registerComponent(describeCharacterController()) && registry.registerComponent(describeCamera()) &&
           registry.registerComponent(makeComponentDescriptor<TagPlayer>("Player"));
}

} // namespace Cthulhu::Scene