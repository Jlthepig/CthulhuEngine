#include "sceneLoader.h"
#include "jsonParser.h"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scene
{
    SceneData SceneLoader::load(const std::string& path, Scene& scene)
    {
        SceneData sceneData;

        // parse the raw JSON - simdjson is isolated in jsonParser.cpp
        auto parsed = JsonParser::parseScene(path);
        if (!parsed.has_value())
        {
            Log::Print("FAILED TO LOAD SCENE: " + path, "SceneLoader", LogType::LOG_ERROR);
            return sceneData;
        }

        scene.clear();

        sceneData.name = parsed->name;
        Log::Print("Loading scene: " + sceneData.name, "SceneLoader", LogType::LOG_INFO);

        // build entities from parsed data
        for (auto& parsedEntity : parsed->entities)
        {
            Entity entity;
            entity.name = parsedEntity.name;
            entity.model = scene.getOrLoadModel(parsedEntity.modelPath);
            entity.transform.setPosition(parsedEntity.position);
            entity.transform.setRotation(parsedEntity.rotation);
            entity.transform.setScale(parsedEntity.scale);
            entity.bounds.min = parsedEntity.boundsMin;
            entity.bounds.max = parsedEntity.boundsMax;

            scene.addEntity(entity);
            Log::Print("Added entity: " + entity.name, "SceneLoader", LogType::LOG_INFO);
        }

        // lights
        sceneData.directionalLight = parsed->directionalLight;
        sceneData.pointLights = parsed->pointLights;

        Log::Print("Scene loaded: " + std::to_string(scene.getEntities().size()) + " entities, "
            + std::to_string(sceneData.pointLights.size()) + " point lights",
            "SceneLoader", LogType::LOG_INFO);

        return sceneData;
    }
}