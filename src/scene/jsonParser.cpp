#include <string>

#include <simdjson.h>

#include "jsonParser.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{

static bool readVec3(simdjson::ondemand::array array,glm::vec3& result)
{
    size_t index = 0;

    for (auto value : array)
    {
        if (index >= 3)
        {
            return false;
        }

        auto number = value.get_double();

        if (number.error())
        {
            return false;
        }

        result[index++] = static_cast<float>(number.value());
    }

    return index == 3;
}

std::optional<ParsedScene> JsonParser::parseScene(const std::string &path)
{
    simdjson::ondemand::parser parser;
    auto json = simdjson::padded_string::load(path);
    if (json.error())
    {
        Log::Print("FAILED TO OPEN SCENE FILE: " + path, "JsonParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto doc = parser.iterate(json);
    ParsedScene result;

    auto nameResult = doc["name"].get_string();
    if (nameResult.error())
    {
        Log::Print("SCENE FILE MUST CONTAIN A NAME name: " + path, "SceneParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto versionResult = doc["format_version"].get_int64();
    if (versionResult.error())
    {
        Log::Print("SCENE FILE IS MISSING format_version: " + path, "SceneParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    const uint64_t version = versionResult.value();
    if (version != SCENE_FORMAT_VERSION)
    {
        Log::Print("UNSUPPORTED SCENE FORMAT VERSION: " + std::to_string(version), "SceneParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    result.name = nameResult.value();
    result.formatVersion = static_cast<uint32_t>(version);

    // entities
    for (auto entityJson : doc["entities"].get_array())
    {
        ParsedEntity entity;

        auto name = entityJson["name"].get_string();
        if (name.error())
        {
            Log::Print("INVALID ENTITY NAME", "SceneParser", LogType::LOG_ERROR);
            return std::nullopt;
        }
        entity.name = name.value();

        auto position = entityJson["position"].get_array();
        if (position.error() || !readVec3(position.value(), entity.position))
        {
            Log::Print("INVALID POSITION: " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto rotation = entityJson["rotation"].get_array();
        if (rotation.error() || !readVec3(rotation.value(), entity.rotation))
        {
            Log::Print("INVALID ROTATION: " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return std::nullopt;
        }
        
        auto scale = entityJson["scale"].get_array();
        if (scale.error() || !readVec3(scale.value(), entity.scale))
        {
            Log::Print("INVALID SCALE: " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto modelResult = entityJson["model"].get_string();
        if (!modelResult.error())
        {
            ParsedMesh mesh;
            mesh.modelPath = std::string(modelResult.value());

            auto boundsResult = entityJson["bounds"].get_object();
            if (!boundsResult.error())
            {
                auto boundsJson = boundsResult.value();

                auto minResult = boundsJson["min"].get_array();
                auto maxResult = boundsJson["max"].get_array();

                if (!minResult.error())
                {
                    readVec3(minResult.value(), mesh.boundsMin);
                }

                if (!maxResult.error())
                {
                    readVec3(maxResult.value(), mesh.boundsMax);
                }
            }

            entity.mesh = std::move(mesh);
        }

        auto physicsResult = entityJson["physics"].get_object();
        if (!physicsResult.error())
        {
            auto physicsJson = physicsResult.value();
            ParsedPhysics physics;
            physics.type = std::string(physicsJson["type"].get_string().value());
            auto halfExtent = physicsJson["half_extent"].get_array();
            if (!halfExtent.error())
            {
                readVec3(halfExtent.value(), physics.halfExtent);
            }

            auto massVal = physicsJson["mass"].get_double();
            if (!massVal.error())
            {
                physics.mass = static_cast<float>(massVal.value());
            }

            entity.physics = physics;
        }

        auto weaponResult = entityJson["weapon"].get_object();
        if (!weaponResult.error())
        {
            auto wj = weaponResult.value();
            ParsedWeapon w;
            auto fr = wj["firerate"].get_double();
            if (!fr.error())
                w.firerate = static_cast<float>(fr.value());
            auto mr = wj["maxrange"].get_double();
            if (!mr.error())
                w.maxRange = static_cast<float>(mr.value());
            entity.weapon = w;
        }

        auto audioResult = entityJson["audio"].get_object();
        if (!audioResult.error())
        {
            auto aj = audioResult.value();
            ParsedAudio a;
            a.file = std::string(aj["file"].get_string().value());
            auto vol = aj["volume"].get_double();
            if (!vol.error())
                a.volume = static_cast<float>(vol.value());
            auto lp = aj["loop"].get_bool();
            if (!lp.error())
                a.loop = lp.value();
            entity.audio = a;
        }

        auto playerVal = entityJson["player"].get_bool();
        if (!playerVal.error())
            entity.player = playerVal.value();

        result.entities.push_back(entity);
    }

    // directional light
    auto dirLightResult = doc["directional_light"].get_object();
    if (dirLightResult.error())
    {
        Log::Print("MISSING OR INVALID DIRECTIONAL LIGHT", "JsonParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto dirLightJson = dirLightResult.value();
    auto directionResult = dirLightJson["direction"].get_array();
    auto colorResult = dirLightJson["color"].get_array();
    auto intensityResult = dirLightJson["intensity"].get_double();

    if (directionResult.error() || colorResult.error() || intensityResult.error() || !readVec3(directionResult.value(), result.directionalLight.direction) ||
        !readVec3(colorResult.value(), result.directionalLight.color))
    {
        Log::Print("INVALID DIRECTIONAL LIGHT DATA", "JsonParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    result.directionalLight.intensity = static_cast<float>(intensityResult.value());

    auto pointLightsResult = doc["point_lights"].get_array();
    if (pointLightsResult.error())
    {
        Log::Print("MISSING OR INVALID POINT LIGHT ARRAY", "JsonParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    for (auto lightValue : pointLightsResult.value())
    {
        auto lightObjectResult = lightValue.get_object();
        if (lightObjectResult.error())
        {
            Log::Print("INVALID POINT LIGHT", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto lightJson = lightObjectResult.value();

        Rendering::PointLight light;

        auto positionResult = lightJson["position"].get_array();
        auto colorResult = lightJson["color"].get_array();
        auto intensityResult = lightJson["intensity"].get_double();
        auto radiusResult = lightJson["radius"]->get_double();

        auto constantResult = lightJson["constant"].get_double();
        auto linearResult = lightJson["linear"].get_double();
        auto quadraticResult = lightJson["quadratic"].get_double();

        if (positionResult.error() || colorResult.error() || intensityResult.error() || radiusResult.error() || constantResult.error() ||
            linearResult.error() || quadraticResult.error() || !readVec3(positionResult.value(), light.position) || 
            !readVec3(colorResult.value(), light.color))
        {
            Log::Print("INVALID POINT LIGHT DATA", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        light.intensity = static_cast<float>(intensityResult.value());
        light.radius = static_cast<float>(radiusResult.value());
        light.constant = static_cast<float>(constantResult.value());
        light.linear = static_cast<float>(linearResult.value());
        light.quadratic = static_cast<float>(quadraticResult.value());

        result.pointLights.push_back(light);
    }

    return result;
}
} // namespace Cthulhu::Scene