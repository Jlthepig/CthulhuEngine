#include "components.h"
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
        auto parsed = JsonParser::parseScene(path); // the parsed information provided by the json parser
        if (!parsed.has_value())
        {
            Log::Print("FAILED TO LOAD SCENE: " + path, "SceneLoader", LogType::LOG_ERROR);
            return;
        }

        scene.setName(parsed->name);

        scene.clear();

        Log::Print("Loading scene: " + parsed->name, "SceneLoader", LogType::LOG_INFO);

        // build entities from parsed information
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
                mesh.modelPath = parsedEntity.modelPath;
                mesh.boundsMin = parsedEntity.boundsMin;
                mesh.boundsMax = parsedEntity.boundsMax;
            }

            // create physics body if there
            if (parsedEntity.physics.has_value())
            {
                auto& p = parsedEntity.physics.value();
                auto& phys = e.ensure<PhysicsComponent>();
                phys.hasBody = true;
                phys.type = p.type;
                phys.halfExtent = p.halfExtent;
                phys.mass = p.mass;

                if (p.type == "static")
                {
                    phys.bodyId = physicsWorld.addStaticBox(parsedEntity.position, p.halfExtent);
                    e.add<TagStatic>();
                }
                else if (p.type == "dynamic")
                {
                    phys.bodyId = physicsWorld.addDynamicBox(parsedEntity.position, p.halfExtent, p.mass);
                }
            }

            if (parsedEntity.weapon.has_value())
            {
                WeaponComponent w;
                w.firerate = parsedEntity.weapon->firerate;
                w.maxRange = parsedEntity.weapon->maxRange;
                e.set(w);
            }

            if (parsedEntity.audio.has_value())
            {
                AudioSourceComponent a;
                a.filePath = parsedEntity.audio->file;
                a.volume = parsedEntity.audio->volume;
                a.loop = parsedEntity.audio->loop;
                e.set(a);
            }

            if (parsedEntity.player) e.add<TagPlayer>();

            e.add<TagActive>();
        }

        
        scene.setDirectionalLight(parsed->directionalLight);
        for (auto& pl :parsed->pointLights) // for every light in scene/parsed information  add a light
        {
            scene.addPointLight(pl);
        }

    }
}