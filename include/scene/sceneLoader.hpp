#pragma once

#include <string>

#include "scene.hpp"

namespace Cthulhu::Assets
{
class AssetRegistry;
}

namespace Cthulhu::Scene
{
class SceneLoader
{
  public:
    static bool load(const std::string &path,
                     Scene &scene,
                     const Assets::AssetRegistry &registry);
};
} // namespace Cthulhu::Scene
