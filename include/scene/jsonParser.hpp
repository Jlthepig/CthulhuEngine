#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "assetId.hpp"
#include "entityId.hpp"
#include "light.hpp"
namespace Cthulhu::Scene
{

inline constexpr uint32_t SCENE_FORMAT_VERSION = 4;

struct ParsedAssetRef
{
    std::string path;
    std::optional<Assets::AssetId> id;
};

using ParsedValue = std::variant<double, bool, std::string, glm::vec3, ParsedAssetRef>;

struct ParsedField
{
    std::string name;
    ParsedValue value;
};

struct ParsedComponent
{
    std::string name;
    std::vector<ParsedField> fields;
};

struct ParsedEntity
{
    EntityId id;
    std::optional<EntityId> parentId;
    std::string name;
    std::vector<ParsedComponent> components;
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