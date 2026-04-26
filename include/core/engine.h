#pragma once

#include "camera.h"
#include "glm.hpp"
#include <functional>
#include <string>

namespace Cthulhu
{
    class Engine
    {
    public:
        using UpdateCallback = std::function<void(float deltaTime)>;
        static void init(const char* title, glm::vec2 resolution);
        static void loadScene(const std::string& path);
        static void setUpdateCallback(UpdateCallback callback);
        static void run();
        static void shutdown();
        static Cthulhu::Scene::Camera* getCamera();
    };
    
    
}