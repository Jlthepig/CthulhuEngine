#pragma once
#include <string>

namespace Cthulhu::Scene {class Scene;}
namespace Cthulhu::Scene
{
    class SceneWriter
    {
        public:
        static bool writeScene(const Scene& scene, const std::string& path);  
    };
}