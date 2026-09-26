#include "sceneLoader.hpp"
#include "components.hpp"
#include "jsonParser.hpp"
#include "project.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
bool SceneLoader::load(const std::string &path, Scene &scene, const Cthulhu::Project::Project &project)
{
    auto parsed = JsonParser::parseScene(path); // the parsed information provided by the json parser
    if (!parsed.has_value())
    {
        Log::Print("FAILED TO LOAD SCENE: " + path, "SceneLoader", LogType::LOG_ERROR);
        return false;
    }

    scene.setName(parsed->name);

    Log::Print("Loading scene: " + parsed->name, "SceneLoader", LogType::LOG_INFO);

    // build entities from parsed information
    for (auto &parsedEntity : parsed->entities)
    {
        auto entity = scene.createEntityWithId(parsedEntity.id, parsedEntity.name);
        if (!entity)
        {
            Log::Print("FAILED TO CREATE ENTITY FROM SCENE: " + entityIdToString(parsedEntity.id),"SceneLoader",LogType::LOG_ERROR);
            return false;
        }

        auto e = *entity;

        auto &transform = e.ensure<TransformComponent>();
        transform.position = parsedEntity.position;
        transform.rotation = parsedEntity.rotation;
        transform.scale = parsedEntity.scale;
        transform.matrixDirty = true;

        if (parsedEntity.mesh)
        {
            const auto& parsedMesh = *parsedEntity.mesh;

            MeshComponent mesh;
            mesh.modelPath = parsedMesh.modelPath;
            mesh.boundsMin = parsedMesh.boundsMin;
            mesh.boundsMax = parsedMesh.boundsMax;
            e.set(mesh);

            auto resolvedModelPath = project.resolveResourcePath(mesh.modelPath);
            if (!resolvedModelPath)
            {
                Log::Print("FAILED TO RESOLVE MODEL RESOURCE: " + mesh.modelPath, "SceneLoader",
                           LogType::LOG_ERROR);
                continue;
            }

            MeshRuntimeComponent runtime;
            runtime.model = scene.getOrLoadModel(mesh.modelPath, *resolvedModelPath);
            e.set(runtime);
        }

        if (parsedEntity.physics)
        {
            const auto& parsedPhysics = *parsedEntity.physics;
            PhysicsComponent physics;
            physics.halfExtent = parsedPhysics.halfExtent;
            physics.mass = parsedPhysics.mass;

            if (parsedPhysics.type == "static")
            {
                physics.type = PhysicsBodyType::Static;
            }
            else if (parsedPhysics.type == "dynamic")
            {
                physics.type = PhysicsBodyType::Dynamic;
            }
            else
            {
                Log::Print("UNKNOWN PHYSICS BODY TYPE", "SceneLoader", LogType::LOG_ERROR);
                return false;
            }

            e.set(physics);
        }

        if (parsedEntity.weapon.has_value())
        {
            WeaponComponent w;
            w.fireRate = parsedEntity.weapon->firerate;
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

        if (parsedEntity.characterController)
        {
            const auto& parsedController = *parsedEntity.characterController;
            CharacterControllerComponent controller;
            controller.gravity = parsedController.gravity;
            controller.jumpVelocity = parsedController.jumpVelocity;
            controller.capsuleRadius = parsedController.capsuleRadius;
            controller.capsuleHeight = parsedController.capsuleHeight;
            controller.maxWalkableSlope = parsedController.maxWalkableSlope;
            controller.maxPushStrength = parsedController.maxPushStrength;
            e.set(controller);
        }

        if (parsedEntity.player)
            e.add<TagPlayer>();

        e.add<TagActive>();
    }

    for (const auto &parsedEntity : parsed->entities)
    {
        if (!parsedEntity.parentId)
        {
            continue;
        }

        if (!scene.isEntityAlive(*parsedEntity.parentId))
        {
            Log::Print("ENTITY REFERENCES MISSING PARENT: " + entityIdToString(*parsedEntity.parentId),"SceneLoader",LogType::LOG_ERROR);
            return false;
        }

        if (!scene.setParent(parsedEntity.id, *parsedEntity.parentId))
        {
            Log::Print("FAILED TO RESTORE ENTITY HIERARCHY","SceneLoader",LogType::LOG_ERROR);
            return false;
        }
    }

    scene.setDirectionalLight(parsed->directionalLight);
    for (auto &pl : parsed->pointLights) // for every light in scene/parsed
                                         // information  add a light
    {
        scene.addPointLight(pl);
    }

    return true;
}
} // namespace Cthulhu::Scene