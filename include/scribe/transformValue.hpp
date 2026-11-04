#pragma once

#include <cmath>

#include <glm.hpp>

#include "components.hpp"
#include "entityId.hpp"

namespace Cthulhu::Scribe
{

struct TransformValue
{
    Scene::EntityId id{};
    glm::vec3 position{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.0f};
    glm::vec3 shear{0.0f};

    bool operator==(const TransformValue &) const = default;
};

inline TransformValue captureTransform(Scene::EntityId id, const Scene::TransformComponent &t) 
{
    return {id, t.position, t.rotation, t.scale, t.shear};
}
inline bool isFiniteTransform(const TransformValue &v) 
{
    for (int a = 0; a < 3; ++a)
        if (!std::isfinite(v.position[a]) || !std::isfinite(v.rotation[a]) ||
            !std::isfinite(v.scale[a]) || !std::isfinite(v.shear[a])) return false;
    return true;
}

} // namespace Cthulhu::Scribe