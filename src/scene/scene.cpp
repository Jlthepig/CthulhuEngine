#include "scene.h"

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

}

