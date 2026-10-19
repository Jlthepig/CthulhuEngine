#pragma once

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/quaternion.hpp>
#include <flecs.h>

#include "components.hpp"

namespace Cthulhu::Scene
{

struct WorldTransform
{
    glm::mat4 matrix{1.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

inline WorldTransform getWorldTransform(flecs::entity entity)
{
    WorldTransform result;
    if (auto parent = entity.parent(); parent && parent.is_alive() && parent.has<TransformComponent>())
    {
        result = getWorldTransform(parent);
    }

    if (!entity.has<TransformComponent>())
    {
        return result;
    }

    const auto &transform = entity.get<TransformComponent>();
    glm::mat4 localMatrix(1.0f);
    localMatrix = glm::translate(localMatrix, transform.position);
    localMatrix = glm::rotate(localMatrix, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    localMatrix = glm::rotate(localMatrix, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    localMatrix = glm::rotate(localMatrix, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    localMatrix = glm::scale(localMatrix, transform.scale);

    glm::mat4 localRotation(1.0f);
    localRotation = glm::rotate(localRotation, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    localRotation = glm::rotate(localRotation, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    localRotation = glm::rotate(localRotation, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

    result.matrix *= localMatrix;
    result.rotation = glm::normalize(result.rotation * glm::quat_cast(localRotation));
    return result;
}

inline glm::vec3 worldPositionToLocal(flecs::entity entity, const glm::vec3 &worldPosition)
{
    auto parent = entity.parent();
    if (!parent || !parent.is_alive() || !parent.has<TransformComponent>())
    {
        return worldPosition;
    }

    const WorldTransform parentTransform = getWorldTransform(parent);
    return glm::vec3(glm::inverse(parentTransform.matrix) * glm::vec4(worldPosition, 1.0f));
}

inline glm::quat worldRotationToLocal(flecs::entity entity, const glm::quat &worldRotation)
{
    auto parent = entity.parent();
    if (!parent || !parent.is_alive() || !parent.has<TransformComponent>())
    {
        return worldRotation;
    }

    return glm::normalize(glm::inverse(getWorldTransform(parent).rotation) * worldRotation);
}

} // namespace Cthulhu::Scene