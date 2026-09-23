#pragma once

#include <optional>
#include <string>
#include <vector>

#include "light.hpp"
namespace Cthulhu::Scene
{

inline constexpr uint32_t SCENE_FORMAT_VERSION = 1;
struct ParsedAudio
{
    std::string file;
    float volume = 1.0f;
    bool loop = false;
};
struct ParsedPhysics
{
    std::string type;
    float mass = 1.0f;
    glm::vec3 halfExtent = glm::vec3(0.5f);
};

struct ParsedMesh
{
    std::string modelPath;
    glm::vec3 boundsMin;
    glm::vec3 boundsMax;
};

struct ParsedWeapon
{
    float firerate = 10.0f;
    float maxRange = 100.0f;
};
struct ParsedEntity
{
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    std::optional<ParsedPhysics> physics;
    std::optional<ParsedWeapon>  weapon;
    std::optional<ParsedAudio>   audio;
    std::optional<ParsedMesh>    mesh;
    bool player = false;
};

struct ParsedScene
{
    std::string name;
    uint32_t formatVersion = SCENE_FORMAT_VERSION;
    std::vector<ParsedEntity> entities;
    Rendering::DirectionalLight directionalLight;
    std::vector<Rendering::PointLight> pointLights;
};
class JsonParser
{
  public:
    static std::optional<ParsedScene> parseScene(const std::string &path);
};
} // namespace Cthulhu::Scene