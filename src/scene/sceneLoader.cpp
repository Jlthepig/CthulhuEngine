#include "pch.h"
#include "sceneLoader.h"
#include "jsonParser.h"
#include "physics.h"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scene
{
    void SceneLoader::load(const std::string& path, Scene& scene, Cthulhu::Physics::PhysicsWorld& physicsWorld)
    {
        // parse the raw JSON - simdjson is isolated in jsonParser.cpp
        auto parsed = JsonParser::parseScene(path);
        if (!parsed.has_value())
        {
            Log::Print("FAILED TO LOAD SCENE: " + path, "SceneLoader", LogType::LOG_ERROR);
            return;
        }

        scene.clear();

        Log::Print("Loading scene: " + parsed->name, "SceneLoader", LogType::LOG_INFO);

        // build entities from parsed data
        for (auto& parsedEntity : parsed->entities)
        {
            auto e = scene.createEntity(parsedEntity.name);

            auto& transform = e.ensure<TransformComponent>();
            transform.position = parsedEntity.position;
            transform.rotation = parsedEntity.rotation;
            transform.scale = parsedEntity.scale;
            transform.matrixDirty = true;

            if (!parsedEntity.modelPath.empty())
            {
                auto& mesh = e.ensure<MeshComponent>();
                mesh.model = scene.getOrLoadModel(parsedEntity.modelPath);
                mesh.boundsMin = parsedEntity.boundsMin;
                mesh.boundsMax = parsedEntity.boundsMax;
            }

            // create a physics body if it is present
            if (parsedEntity.physics.has_value())
            {
                auto& p = parsedEntity.physics.value();
                auto& phys = e.ensure<PhysicsComponent>();
                phys.hasBody = true;

                if (p.type == "static")
                {
                    phys.bodyId = physicsWorld.addStaticBox(
                        parsedEntity.position, p.halfExtent);
                    e.add<TagStatic>();
                }
                else if (p.type == "dynamic")
                {
                    phys.bodyId = physicsWorld.addDynamicBox(
                        parsedEntity.position, p.halfExtent, p.mass);
                }
            }

            e.add<TagActive>();
        }

        // lights
        scene.setDirectionalLight(parsed->directionalLight);
        for (auto& pl :parsed->pointLights)
        {
            scene.addPointLight(pl);
        }

    }
}