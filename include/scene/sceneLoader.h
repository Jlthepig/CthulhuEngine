#pragma once
#include "physics.h"
#include "scene.h"
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
    static bool load(const std::string &path, Scene &scene, Cthulhu::Physics::PhysicsWorld &physicsWorld,
                     const Project::Project &project);
};
} // namespace Cthulhu::Scene