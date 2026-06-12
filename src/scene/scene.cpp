#include "pch.h"
#include "scene.h"
#include "log_utils.hpp"
#include "modelLoader.h"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
    flecs::entity Scene::createEntity(const std::string& name)
    {
        auto e = world.entity(name.c_str());
        e.set(TransformComponent{});
        Log::Print("Created entity: " + name, "Scene", LogType::LOG_INFO);
        return e;
    }

    Rendering::Model* Scene::getOrLoadModel(const std::string& path)
    {
        auto it = modelCache.find(path);
        if (it != modelCache.end())
        {
            Log::Print("Resuing model from cache: " + path,"Scene", LogType::LOG_INFO);
            return &it->second;
        }
        else
        {
            Log::Print("Loading model from file: " + path,"Scene", LogType::LOG_INFO);
            Rendering::Model model = Rendering::ModelLoader::loadGltf(path);
            modelCache[path] = std::move(model);
            return &modelCache[path];
        }
    }

    void Scene::clear()
    {
       world.delete_with<TransformComponent>();

        for (auto& [path, model] : modelCache)
        {
            model.destroy();
        }
        modelCache.clear();
        nextId = 0;
        Log::Print("Scene cleared", "Scene", LogType::LOG_INFO);
    }
}

