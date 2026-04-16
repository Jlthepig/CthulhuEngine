#include "transform.h"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"



namespace Cthulhu::Scene
{
    glm::mat4 Transform::getModelMatrix()
    {
        if (matrixDirty)
        {
            cachedModelMatrix = glm::mat4(1.0f);
            cachedModelMatrix = glm::translate(cachedModelMatrix, position);
            cachedModelMatrix = glm::rotate(cachedModelMatrix, rotation.x, glm::vec3(1.0f,0.0f,0.0f));
            cachedModelMatrix = glm::rotate(cachedModelMatrix, rotation.y, glm::vec3(0.0f,1.0f,0.0f));
            cachedModelMatrix = glm::rotate(cachedModelMatrix, rotation.z, glm::vec3(0.0f,0.0f,1.0f));
            cachedModelMatrix = glm::scale(cachedModelMatrix, scale);
            matrixDirty = false;
        }
        return cachedModelMatrix;
    }
    
    void Transform::setPosition(const glm::vec3& p)
    {
        position = p;
        matrixDirty = true;
    }

    void Transform::setRotation(const glm::vec3& r)
    {
        rotation = r;
        matrixDirty = true;
    }

    void Transform::setScale(const glm::vec3& s)
    {
        scale = s;
        matrixDirty = true;
    }

}