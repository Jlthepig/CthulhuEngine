#pragma once
#include "scene.h"
#include "light.h"
#include "physics.h"
#include <string>
namespace Cthulhu::Scene
{   
    class SceneLoader
    {
    public:
        // Loads a .scene JSON file into the scene and returns light data
        static void load(const std::string& path,Scene& scene, Cthulhu::Physics::PhysicsWorld& physicsWorld);
    };
}