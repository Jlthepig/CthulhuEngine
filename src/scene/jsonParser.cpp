#include <string>
#include <unordered_set>

#include <simdjson.h>

#include "jsonParser.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{

static bool readVec3(simdjson::ondemand::array array, glm::vec3 &result)
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

using EntityJson = simdjson::simdjson_result<simdjson::ondemand::value>;

static bool readValue(simdjson::simdjson_result<simdjson::ondemand::value> json, ParsedValue& out)
{
    simdjson::ondemand::json_type type;
    if (json.type().get(type))
    {
        return false;
    }

    switch (type)
    {
        case simdjson::ondemand::json_type::number:
        {
            double number = 0.0;
            if (json.get_double().get(number))
            {
                return false;
            }
            out = number;
            return true;
        }
        case simdjson::ondemand::json_type::boolean:
        {
            bool flag = false;
            if (json.get_bool().get(flag))
            {
                return false;
            }
            out = flag;
            return true;
        }
        case simdjson::ondemand::json_type::string:
        {
            std::string_view text;
            if (json.get_string().get(text))
            {
                return false;
            }
            out = std::string(text);
            return true;
        }
        case simdjson::ondemand::json_type::array:
        {
            simdjson::ondemand::array array;
            glm::vec3 vector{};
            if (json.get_array().get(array) || !readVec3(array, vector))
            {
                return false;
            }
            out = vector;
            return true;
        }
        case simdjson::ondemand::json_type::object:
        {
            simdjson::ondemand::object object;
            std::string_view assetPath;
            if (json.get_object().get(object) || object["path"].get_string().get(assetPath))
            {
                return false;
            }

            ParsedAssetRef ref;
            ref.path = std::string(assetPath);

            std::string_view idText;
            if (!object["id"].get_string().get(idText))
            {
                ref.id = Assets::assetIdFromString(idText);
                if (!ref.id)
                {
                    return false;
                }
            }

            out = std::move(ref);
            return true;
        }
        default:
            return false;
    }
}

static bool readComponents(EntityJson& entityJson, ParsedEntity& entity)
{
    simdjson::ondemand::object components;
    if (entityJson["components"].get_object().get(components))
    {
        Log::Print("ENTITY HAS NO components: " + entity.name, "SceneParser", LogType::LOG_ERROR);
        return false;
    }

    for (auto componentJson : components)
    {
        std::string_view componentName;
        simdjson::ondemand::object fieldsJson;
        if (componentJson.unescaped_key().get(componentName) || componentJson.value().get_object().get(fieldsJson))
        {
            Log::Print("INVALID COMPONENT ON: " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return false;
        }

        ParsedComponent component;
        component.name = std::string(componentName);

        for (auto fieldJson : fieldsJson)
        {
            std::string_view fieldName;
            if (fieldJson.unescaped_key().get(fieldName))
            {
                Log::Print("INVALID FIELD IN " + component.name + " ON: " + entity.name, "SceneParser", LogType::LOG_ERROR);
                return false;
            }

            ParsedField field;
            field.name = std::string(fieldName);
            if (!readValue(fieldJson.value(), field.value))
            {
                Log::Print("INVALID VALUE FOR " + component.name + "." + field.name + " ON: " + entity.name,
                           "SceneParser", LogType::LOG_ERROR);
                return false;
            }

            component.fields.push_back(std::move(field));
        }

        entity.components.push_back(std::move(component));
    }

    return true;
}

static void readLegacyNumber(simdjson::ondemand::object& json, const char* key, const char* field, ParsedComponent& component)
{
    double number = 0.0;
    if (!json[key].get_double().get(number))
    {
        component.fields.push_back({field, number});
    }
}

static bool readLegacyComponents(EntityJson& entityJson, ParsedEntity& entity)
{
    ParsedComponent transform{"Transform", {}};
    for (const char* key : {"position", "rotation", "scale"})
    {
        glm::vec3 vector{};
        auto array = entityJson[key].get_array();
        if (array.error() || !readVec3(array.value(), vector))
        {
            Log::Print("INVALID " + std::string(key) + ": " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return false;
        }
        transform.fields.push_back({key, vector});
    }
    entity.components.push_back(std::move(transform));

    std::string_view model;
    if (!entityJson["model"].get_string().get(model))
    {
        ParsedAssetRef ref{std::string(model), std::nullopt};

        std::string_view modelId;
        if (!entityJson["model_id"].get_string().get(modelId))
        {
            ref.id = Assets::assetIdFromString(modelId);
            if (!ref.id)
            {
                Log::Print("INVALID model_id: " + entity.name, "SceneParser", LogType::LOG_ERROR);
                return false;
            }
        }

        ParsedComponent mesh{"Mesh", {}};
        mesh.fields.push_back({"modelPath", std::move(ref)});
        entity.components.push_back(std::move(mesh));
    }

    simdjson::ondemand::object physicsJson;
    if (!entityJson["physics"].get_object().get(physicsJson))
    {
        std::string_view type;
        if (physicsJson["type"].get_string().get(type))
        {
            Log::Print("PHYSICS IS MISSING type: " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return false;
        }

        // old files used lowercase names; anything else is passed on and rejected by the loader
        std::string typeName(type);
        if (typeName == "static")
        {
            typeName = "Static";
        }
        else if (typeName == "dynamic")
        {
            typeName = "Dynamic";
        }

        ParsedComponent physics{"Physics", {}};
        physics.fields.push_back({"type", typeName});

        glm::vec3 halfExtent{};
        auto halfExtentJson = physicsJson["half_extent"].get_array();
        if (!halfExtentJson.error() && readVec3(halfExtentJson.value(), halfExtent))
        {
            physics.fields.push_back({"halfExtent", halfExtent});
        }

        readLegacyNumber(physicsJson, "mass", "mass", physics);
        entity.components.push_back(std::move(physics));
    }

    simdjson::ondemand::object weaponJson;
    if (!entityJson["weapon"].get_object().get(weaponJson))
    {
        ParsedComponent weapon{"Weapon", {}};
        readLegacyNumber(weaponJson, "firerate", "fireRate", weapon);
        readLegacyNumber(weaponJson, "maxrange", "maxRange", weapon);
        entity.components.push_back(std::move(weapon));
    }

    simdjson::ondemand::object audioJson;
    if (!entityJson["audio"].get_object().get(audioJson))
    {
        std::string_view file;
        if (audioJson["file"].get_string().get(file))
        {
            Log::Print("AUDIO IS MISSING file: " + entity.name, "SceneParser", LogType::LOG_ERROR);
            return false;
        }

        ParsedAssetRef ref{std::string(file), std::nullopt};

        std::string_view fileId;
        if (!audioJson["file_id"].get_string().get(fileId))
        {
            ref.id = Assets::assetIdFromString(fileId);
            if (!ref.id)
            {
                Log::Print("INVALID audio file_id: " + entity.name, "SceneParser", LogType::LOG_ERROR);
                return false;
            }
        }

        ParsedComponent audio{"AudioSource", {}};
        audio.fields.push_back({"filePath", std::move(ref)});
        readLegacyNumber(audioJson, "volume", "volume", audio);

        bool loop = false;
        if (!audioJson["loop"].get_bool().get(loop))
        {
            audio.fields.push_back({"loop", loop});
        }

        entity.components.push_back(std::move(audio));
    }

    simdjson::ondemand::object controllerJson;
    if (!entityJson["character_controller"].get_object().get(controllerJson))
    {
        ParsedComponent controller{"CharacterController", {}};
        readLegacyNumber(controllerJson, "gravity", "gravity", controller);
        readLegacyNumber(controllerJson, "jump_velocity", "jumpVelocity", controller);
        readLegacyNumber(controllerJson, "capsule_radius", "capsuleRadius", controller);
        readLegacyNumber(controllerJson, "capsule_height", "capsuleHeight", controller);
        readLegacyNumber(controllerJson, "max_walkable_slope", "maxWalkableSlope", controller);
        readLegacyNumber(controllerJson, "max_push_strength", "maxPushStrength", controller);
        entity.components.push_back(std::move(controller));
    }

    bool player = false;
    if (!entityJson["player"].get_bool().get(player) && player)
    {
        entity.components.push_back({"Player", {}});
    }

    return true;
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

    // << version >>

    auto versionResult = doc["format_version"].get_int64();
    if (versionResult.error())
    {
        Log::Print("SCENE FILE IS MISSING format_version: " + path, "SceneParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    const uint64_t version = versionResult.value();
    if (version < 2 || version > 4)
    {
        Log::Print("UNSUPPORTED SCENE FORMAT VERSION: " + std::to_string(version), "SceneParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    result.formatVersion = static_cast<uint32_t>(version);

    auto nameResult = doc["name"].get_string();
    if (nameResult.error())
    {
        Log::Print("SCENE FILE MUST CONTAIN A NAME name: " + path, "SceneParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    result.name = nameResult.value();

    // << entities >>
    std::unordered_set<EntityId, EntityIdHash> parsedEntityIds;
    for (auto entityJson : doc["entities"].get_array())
    {
        ParsedEntity entity;

        auto idResult = entityJson["id"].get_string();
        if (idResult.error())
        {
            Log::Print("ENTITY IS MISSING ID", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto parsedId = entityIdFromString(idResult.value());
        if (!parsedId)
        {
            Log::Print("ENTITY HAS INVALID ID", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }
        entity.id = *parsedId;

        if (!parsedEntityIds.insert(entity.id).second)
        {
            Log::Print("SCENE CONTAINS DUPLICATE ENTITY ID: " + entityIdToString(entity.id), "JsonParser",
                       LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto parentResult = entityJson["parent"].get_string();
        if (!parentResult.error())
        {
            auto parsedParent = entityIdFromString(parentResult.value());
            if (!parsedParent)
            {
                Log::Print("ENTITY HAS INVALID PARENT ID", "JsonParser", LogType::LOG_ERROR);
                return std::nullopt;
            }

            if (*parsedParent == entity.id)
            {
                Log::Print("ENTITY CANNOT BE ITS OWN PARENT", "JsonParser", LogType::LOG_ERROR);
                return std::nullopt;
            }

            entity.parentId = *parsedParent;
        }

        auto name = entityJson["name"].get_string();
        if (name.error())
        {
            Log::Print("INVALID ENTITY NAME", "SceneParser", LogType::LOG_ERROR);
            return std::nullopt;
        }
        entity.name = name.value();

        const bool componentsRead = version >= 4 ? readComponents(entityJson, entity) : readLegacyComponents(entityJson, entity);
        if (!componentsRead)
        {
            return std::nullopt;
        }

        result.entities.push_back(std::move(entity));
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
    if (directionResult.error() || !readVec3(directionResult.value(), result.directionalLight.direction))
    {
        Log::Print("INVALID DIRECTIONAL LIGHT DIRECTION", "JsonParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto colorResult = dirLightJson["color"].get_array();
    if (colorResult.error() || !readVec3(colorResult.value(), result.directionalLight.color))
    {
        Log::Print("INVALID DIRECTIONAL LIGHT COLOR", "JsonParser", LogType::LOG_ERROR);
        return std::nullopt;
    }

    auto intensityResult = dirLightJson["intensity"].get_double();
    if (intensityResult.error())
    {
        Log::Print("INVALID DIRECTIONAL LIGHT INTENSITY", "JsonParser", LogType::LOG_ERROR);
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
        if (positionResult.error() || !readVec3(positionResult.value(), light.position))
        {
            Log::Print("INVALID POINT LIGHT POSITION", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto colorResult = lightJson["color"].get_array();
        if (colorResult.error() || !readVec3(colorResult.value(), light.color))
        {
            Log::Print("INVALID POINT LIGHT COLOR", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        auto readFloat = [&](std::string_view key, float &out, bool required) -> bool {
            auto value = lightJson[key].get_double();
            if (value.error())
            {
                return !required;
            }
            out = static_cast<float>(value.value());
            return true;
        };

        if (!readFloat("intensity", light.intensity, true) || !readFloat("radius", light.radius, false) ||
            !readFloat("constant", light.constant, true) || !readFloat("linear", light.linear, true) ||
            !readFloat("quadratic", light.quadratic, true))
        {
            Log::Print("INVALID POINT LIGHT DATA", "JsonParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        result.pointLights.push_back(light);
    }

    return result;
}
} // namespace Cthulhu::Scene