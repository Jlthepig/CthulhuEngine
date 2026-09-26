#pragma once
#include <cstdint>
#include <string>
#include <glm.hpp>
#include "entityId.hpp"

namespace Cthulhu::Rendering { struct Model; }
namespace JPH { class CharacterVirtual; }

namespace Cthulhu::Scene {

struct EntityIdentityComponent {
    EntityId id{};
};

struct NameComponent {
    std::string name{"Entity"};
};

struct AudioSourceComponent {
    std::string filePath{""};
    float volume{1.0f};
    bool loop{false};
};

struct AudioSourceRuntimeComponent {
    bool playRequested{false};
    bool stopRequested{false};
    bool isPlaying{false};
    uint32_t soundInstanceId{0};
};

struct CameraComponent {
    glm::vec3 front{0.0f, 0.0f, -1.0f};
};

struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    bool matrixDirty{true};
    glm::mat4 cachedModelMatrix{1.0f};
    glm::mat4 cachedNormalMatrix{1.0f};
};

struct MeshComponent {
    std::string modelPath{""};
    glm::vec3 boundsMin{-1.0f};
    glm::vec3 boundsMax{1.0f};
};

struct MeshRuntimeComponent {
    Rendering::Model* model{nullptr};
};

enum class PhysicsBodyType : uint8_t { Static, Dynamic };

struct PhysicsComponent {
    PhysicsBodyType type{PhysicsBodyType::Static};
    glm::vec3 halfExtent{0.5f};
    float mass{1.0f}; 
};

struct PhysicsRuntimeComponent {
    uint32_t bodyId{0};
};

struct CharacterControllerComponent {
    float gravity{-9.81f};
    float jumpVelocity{5.0f};
    float capsuleRadius{0.3f};
    float capsuleHeight{2.0f};
    float maxWalkableSlope{45.0f};
    float maxPushStrength{100.0f};
};

struct CharacterControllerRuntimeComponent {
    JPH::CharacterVirtual* character{nullptr};
    float verticalVelocity{0.0f};
    glm::vec3 prevPos{0.0f};
    glm::vec3 currentPos{0.0f};
    glm::vec3 pendingMove{0.0f};
    bool pendingJump{false};
};

struct WeaponComponent {
    float fireRate{10.0f};
    float maxRange{100.0f};
};

struct WeaponRuntimeComponent {
    float timeSinceLastShot{0.0f};
    bool wantsToFire{false};
};

struct TagActive {};
struct TagPlayer {};

} // namespace Cthulhu::Scene
