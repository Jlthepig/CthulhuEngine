#pragma once

#include <string>

#include "scene.hpp"
namespace Cthulhu::Project
{
class Project;
}
namespace Cthulhu::Scene
{
class SceneLoader
{
  public:
    static bool load(const std::string &path, Scene &scene, const Project::Project &project);
};
} // namespace Cthulhu::Scene