#include "transform.h"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"



namespace Cthulhu::Scene
{
    glm::mat4 Transform::getModelMatrix() const
    {
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, position);
        modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f,0.0f,0.0f));
        modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f,1.0f,0.0f));
        modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f,0.0f,1.0f));
        modelMatrix = glm::scale(modelMatrix, scale);

        return modelMatrix;
    }
}