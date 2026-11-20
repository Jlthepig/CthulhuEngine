#pragma once

#include <string>

namespace Cthulhu::Scene
{
class Scene;
}

namespace Cthulhu::Assets
{
class AssetRegistry;
}

namespace Cthulhu::Scene
{
class SceneWriter
{
  public:
    static bool writeScene(const Scene &scene, const std::string &path, const Assets::AssetRegistry &registry, const std::string &name);
};
} // namespace Cthulhu::Scene
