#pragma once

#include "glm.hpp"

namespace Cthulhu::Scene
{
    struct Transform
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);

        glm::mat4 cachedModelMatrix = glm::mat4(1.0f);
        bool matrixDirty = true;
        glm::mat4 getModelMatrix();

        void setPosition(const glm::vec3& p);
        void setRotation(const glm::vec3& r);
        void setScale(const glm::vec3& s);
    };
}