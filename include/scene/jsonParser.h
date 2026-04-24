#pragma once

#include "glm.hpp"
#include "light.h"
#include <string>
#include <vector>
#include <optional>

namespace Cthulhu::Scene
{
    struct ParsedEntity
    {
        std::string name;
        std::string modelPath;
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::vec3 boundsMin = glm::vec3(-1.0f);
        glm::vec3 boundsMax = glm::vec3(1.0f);
    };

    struct ParsedScene
    {
        std::string name;
        std::vector<ParsedEntity> entities;
        Rendering::DirectionalLight directionalLight;
        std::vector<Rendering::PointLight> pointLights;
    };

    class JsonParser
    {
    public:
        static std::optional<ParsedScene> parseScene(const std::string& path);
        
    };
}