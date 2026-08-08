#pragma once

#include "camera.h"
#include "fwd.hpp"
#include "glm.hpp"
#include <cstdint>
#include <string>


namespace Cthulhu::Rendering { struct Model; }
namespace JPH { class CharacterVirtual; }
namespace Cthulhu::Scene
{
    struct AudioSourceComponent
    {
        std::string filePath;
        float volume = 1.0f;
        bool loop = false;

        bool playTrigger = false;
        bool stopTrigger = false;

        bool isPlaying = false;
        uint32_t soundInstanceId = 0; // tracked by audio system
    };
    struct CameraComponent
    {
        glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    };
    struct TransformComponent
    {
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};

        bool matrixDirty = true;
        glm::mat4 cachedModelMatrix = glm::mat4(1.0f);
        glm::mat4 cachedNormalMatrix = glm::mat4(1.0f);
    };

    struct MeshComponent
    {
        Cthulhu::Rendering::Model* model = nullptr;
        std::string modelPath;
        glm::vec3 boundsMin = glm::vec3(-1.0f);
        glm::vec3 boundsMax = glm::vec3(1.0f);
    };

    struct PhysicsComponent
    {
        uint32_t bodyId = 0;
        bool hasBody = false;
        std::string type = "static";
        glm::vec3 halfExtent = glm::vec3(0.5f);
        float mass = 1.0f;
    };

    struct CharacterControllerComponent
    {
        JPH::CharacterVirtual* character = nullptr;

        float verticalVelocity = 0.0f;
        glm::vec3 prevPos = glm::vec3(0.0f);
        glm::vec3 currentPos = glm::vec3(0.0f);

        glm::vec3 pendingMove = glm::vec3(0.0f);
        bool pendingJump = false;
    };

    struct WeaponComponent
    {
        float firerate = 10.0f;     // how fast the gun shoots
        float maxRange = 100.0f;    // max distance in meters

        // managed by engine
        float timeSinceLastShot = 0.0;

        // Input (managed by game)
        bool wantsToFire = false;
    };

    struct TagActive {};
    struct TagStatic {};
    struct TagPlayer {};
}