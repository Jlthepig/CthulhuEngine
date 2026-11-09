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

    d.fields.push_back(
        makeField<TransformComponent, &TransformComponent::position>(
            "position", FieldType::Vec3, 0.0f, 0.0f, 0.1f));

    auto rotation =
        makeField<TransformComponent, &TransformComponent::rotation>(
            "rotation", FieldType::Vec3);
    rotation.hint = FieldHint::Degrees;
    d.fields.push_back(std::move(rotation));

    d.fields.push_back(
        makeField<TransformComponent, &TransformComponent::scale>(
            "scale", FieldType::Vec3, 0.0f, 0.0f, 0.1f));

    auto shear =
        makeField<TransformComponent, &TransformComponent::shear>(
            "shear", FieldType::Vec3, 0.0f, 0.0f, 0.1f);
    shear.hint = FieldHint::Hidden;
    d.fields.push_back(std::move(shear));

    return d;
}

ComponentDescriptor describeMesh()
{
    auto d = makeComponentDescriptor<MeshComponent>("Mesh");

    auto model =
        makeField<MeshComponent, &MeshComponent::modelPath>(
            "modelPath", FieldType::AssetRef);
    model.assetType = Assets::AssetType::Model;
    d.fields.push_back(std::move(model));

    return d;
}

ComponentDescriptor describePhysics()
{
    auto d = makeComponentDescriptor<PhysicsComponent>("Physics");

    auto type = makeField(
        "type",
        FieldType::Enum,
        [](flecs::entity e) -> FieldValue { return static_cast<int>(e.get<PhysicsComponent>().type); },
        [](flecs::entity e, const FieldValue &v) {
            auto c = e.get<PhysicsComponent>();
            c.type = static_cast<PhysicsBodyType>(std::get<int>(v));
            e.set(c);
        });
    type.enumNames = {"Static", "Dynamic"}; // HAS TO match PhysicsBodyType order >> IMPORTANT
    d.fields.push_back(std::move(type));

    d.fields.push_back(
        makeField<PhysicsComponent, &PhysicsComponent::halfExtent>(
            "halfExtent", FieldType::Vec3, 0.0f, 0.0f, 0.05f));

    d.fields.push_back(
        makeField<PhysicsComponent, &PhysicsComponent::mass>(
            "mass", FieldType::Float, 0.001f, unbounded, 0.1f));

    return d;
}

ComponentDescriptor describeWeapon()
{
    auto d = makeComponentDescriptor<WeaponComponent>("Weapon");

    d.fields.push_back(
        makeField<WeaponComponent, &WeaponComponent::fireRate>(
            "fireRate", FieldType::Float, 0.0f, unbounded, 0.1f));

    d.fields.push_back(
        makeField<WeaponComponent, &WeaponComponent::maxRange>(
            "maxRange", FieldType::Float, 0.0f, unbounded, 1.0f));

    return d;
}

ComponentDescriptor describeAudioSource()
{
    auto d = makeComponentDescriptor<AudioSourceComponent>("AudioSource");

    auto file =
        makeField<AudioSourceComponent, &AudioSourceComponent::filePath>(
            "filePath", FieldType::AssetRef);
    file.assetType = Assets::AssetType::Audio;
    d.fields.push_back(std::move(file));

    d.fields.push_back(
        makeField<AudioSourceComponent, &AudioSourceComponent::volume>(
            "volume", FieldType::Float, 0.0f, unbounded, 0.05f));

    d.fields.push_back(
        makeField<AudioSourceComponent, &AudioSourceComponent::loop>(
            "loop", FieldType::Bool));

    return d;
}

ComponentDescriptor describeCharacterController()
{
    auto d =
        makeComponentDescriptor<CharacterControllerComponent>("CharacterController");

    d.fields.push_back(
        makeField<CharacterControllerComponent, &CharacterControllerComponent::gravity>(
            "gravity", FieldType::Float, 0.0f, 0.0f, 0.1f));

    d.fields.push_back(
        makeField<CharacterControllerComponent, &CharacterControllerComponent::jumpVelocity>(
            "jumpVelocity", FieldType::Float, 0.0f, unbounded, 0.1f));

    d.fields.push_back(
        makeField<CharacterControllerComponent, &CharacterControllerComponent::capsuleRadius>(
            "capsuleRadius", FieldType::Float, 0.01f, unbounded, 0.01f));

    d.fields.push_back(
        makeField<CharacterControllerComponent, &CharacterControllerComponent::capsuleHeight>(
            "capsuleHeight", FieldType::Float, 0.01f, unbounded, 0.05f));

    d.fields.push_back(
        makeField<CharacterControllerComponent, &CharacterControllerComponent::maxWalkableSlope>(
            "maxWalkableSlope", FieldType::Float, 0.0f, 90.0f, 1.0f));

    d.fields.push_back(
        makeField<CharacterControllerComponent, &CharacterControllerComponent::maxPushStrength>(
            "maxPushStrength", FieldType::Float, 0.0f, unbounded, 1.0f));

    return d;
}

ComponentDescriptor describeCamera()
{
    auto d = makeComponentDescriptor<CameraComponent>("Camera");

    d.fields.push_back(
        makeField<CameraComponent, &CameraComponent::front>(
            "front", FieldType::Vec3));

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
