#pragma once
#include "scene.h"
#include "physics.h"
#include <string>

namespace Cthulhu::Project
{
    class Project;
}
namespace Cthulhu::Scene
{   
    class SceneLoader
    {
    public:
        static bool load(const std::string& path, Scene& scene, Cthulhu::Physics::PhysicsWorld& physicsWorld, const Project::Project& project);
    };
}