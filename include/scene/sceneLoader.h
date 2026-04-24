#pragma once
#include "scene.h"
#include "light.h"
#include <string>
namespace Cthulhu::Scene
{

    struct SceneData
    {
        std::string name;
        Rendering::DirectionalLight directionalLight;
        std::vector<Rendering::PointLight> pointLights;
    };


    class SceneLoader
    {
    public:
        // Loads a .scene JSON file into the scene and returns light data
        static SceneData load(const std::string& path,Scene& scene);
    };
}