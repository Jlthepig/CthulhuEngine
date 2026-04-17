#pragma once

#include "glm.hpp"

namespace Cthulhu::Scene
{
    struct Transform
    {
        public:
        glm::vec3 getPosition() const { return position; }
        glm::vec3 getRotation() const { return rotation; }
        glm::vec3 getScale() const { return scale; }
        glm::mat4 getModelMatrix();

        void setPosition(const glm::vec3& p);
        void setRotation(const glm::vec3& r);
        void setScale(const glm::vec3& s);
        
        private:
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        glm::mat4 cachedModelMatrix = glm::mat4(1.0f);
        bool matrixDirty = true;
    };
}