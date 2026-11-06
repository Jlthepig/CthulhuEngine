#pragma once

#include <cmath>
#include <optional>

#include <flecs.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtx/quaternion.hpp>

#include "components.hpp"

namespace Cthulhu::Scene
{

struct WorldTransform
{
    glm::mat4 matrix{1.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

inline glm::mat4 localRotationMatrix(const glm::vec3 &rotation)
{
    glm::mat4 matrix(1.0f);
    matrix = glm::rotate(matrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    matrix = glm::rotate(matrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    matrix = glm::rotate(matrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return matrix;
}

inline glm::mat4 composeLocalMatrix(const TransformComponent &transform, const glm::mat4 &rotation)
{
    glm::mat4 matrix;
    matrix[0] = rotation[0] * transform.scale.x;
    matrix[1] = (rotation[1] + rotation[0] * transform.shear.x) * transform.scale.y;
    matrix[2] = (rotation[2] + rotation[0] * transform.shear.y + rotation[1] * transform.shear.z) * transform.scale.z;
    matrix[3] = glm::vec4(transform.position, 1.0f);
    return matrix;
}

inline bool isLocalMatrixRepresentable(const TransformComponent &t)
{
    const glm::mat4 m = composeLocalMatrix(t, localRotationMatrix(t.rotation));
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r])) return false;
    return true;
}

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
    const glm::mat4 rotation = localRotationMatrix(transform.rotation);

    result.matrix *= composeLocalMatrix(transform, rotation);
    result.rotation = glm::normalize(result.rotation * glm::quat_cast(rotation));
    return result;
}

inline std::optional<glm::vec3> worldPositionToLocal(flecs::entity entity, const glm::vec3 &worldPosition)
{
    if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y) || !std::isfinite(worldPosition.z))
    {
        return std::nullopt;
    }

    const auto parent = entity.parent();
    if (!parent || !parent.is_alive() || !parent.has<TransformComponent>())
        return worldPosition;

    const WorldTransform parentTransform = getWorldTransform(parent);

    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            if (!std::isfinite(parentTransform.matrix[column][row]))
                return std::nullopt;
        }
    }

    const float determinant = glm::determinant(glm::mat3(parentTransform.matrix));

    if (!std::isfinite(determinant) || determinant == 0.0f)
        return std::nullopt;

    const glm::vec4 local = glm::inverse(parentTransform.matrix) * glm::vec4(worldPosition, 1.0f);

    if (!std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z) || !std::isfinite(local.w))
    {
        return std::nullopt;
    }

    return glm::vec3(local);
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
