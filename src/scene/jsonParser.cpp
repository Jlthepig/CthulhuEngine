#include "jsonParser.h"
#include "simdjson.h"
#include "log_utils.hpp"
#include <string>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scene
{
    
    
    static glm::vec3 readVec3(simdjson::ondemand::array arr)
    {
        glm::vec3 result(0.0f);
        int i = 0;
        for (auto val : arr)
        {
            if (i == 0) result.x = static_cast<float>(val.get_double().value());
            if (i == 1) result.y = static_cast<float>(val.get_double().value());
            if (i == 2) result.z = static_cast<float>(val.get_double().value());
            i++;
        }
        return result;
    }

    std::optional<ParsedScene> JsonParser::parseScene(const std::string& path)
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

        // scene name
        result.name = std::string(doc["name"].get_string().value());

        // entities
        for (auto entityJson : doc["entities"].get_array())
        {
            ParsedEntity entity;
            entity.name = std::string(entityJson["name"].get_string().value());
            entity.modelPath = std::string(entityJson["model"].get_string().value());
            entity.position = readVec3(entityJson["position"].get_array().value());
            entity.rotation = readVec3(entityJson["rotation"].get_array().value());
            entity.scale = readVec3(entityJson["scale"].get_array().value());

            auto boundsJson = entityJson["bounds"].get_object().value();
            entity.boundsMin = readVec3(boundsJson["min"].get_array().value());
            entity.boundsMax = readVec3(boundsJson["max"].get_array().value());

            auto physicsResult = entityJson["physics"].get_object();
            if (!physicsResult.error())
            {
                auto physicsJson = physicsResult.value();
                ParsedPhysics physics;
                physics.type = std::string(physicsJson["type"].get_string().value());
                physics.halfExtent = readVec3(physicsJson["half_extent"].get_array().value());

                auto massVal = physicsJson["mass"].get_double();
                if (!massVal.error())
                {
                    physics.mass = static_cast<float>(massVal.value());
                }

                entity.physics = physics;
            }

            result.entities.push_back(entity);
        }

        // directional light
        auto dirLightJson = doc["directional_light"].get_object().value();
        result.directionalLight.direction = readVec3(dirLightJson["direction"].get_array().value());
        result.directionalLight.color = readVec3(dirLightJson["color"].get_array().value());
        result.directionalLight.intensity = static_cast<float>(dirLightJson["intensity"].get_double().value());

        // point lights
        for (auto lightJson : doc["point_lights"].get_array())
        {
            Rendering::PointLight light;
            light.position = readVec3(lightJson["position"].get_array().value());
            light.color = readVec3(lightJson["color"].get_array().value());
            light.intensity = static_cast<float>(lightJson["intensity"].get_double().value());
            light.constant = static_cast<float>(lightJson["constant"].get_double().value());
            light.linear = static_cast<float>(lightJson["linear"].get_double().value());
            light.quadratic = static_cast<float>(lightJson["quadratic"].get_double().value());
            result.pointLights.push_back(light);
        }

        return result;
    }
}