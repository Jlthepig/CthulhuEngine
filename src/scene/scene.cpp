#include "scene.h"
#include "log_utils.hpp"
#include "modelLoader.h"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
    Entity& Scene::addEntity(Entity entity)
    {   
        entity.id = nextId++;
        entities.push_back(std::move(entity));
        return entities.back();
    }

    std::vector<Entity>& Scene::getEntities()
    {
        return entities;
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
        entities.clear();

        for (auto& [path, model] : modelCache)
        {
            model.destroy();
        }
        modelCache.clear();
        nextId = 0;
        Log::Print("Scene cleared", "Scene", LogType::LOG_INFO);
    }
}

