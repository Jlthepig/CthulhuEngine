#pragma once

#include <glm.hpp>

namespace Cthulhu::Editor
{

struct PickingRay
{
    glm::vec3 origin{};
    glm::vec3 direction{};
};

[[nodiscard]] bool rayIntersectsBounds(const PickingRay &ray,
                                       const glm::vec3 &minimum,
                                       const glm::vec3 &maximum,
                                       float maxDistance);
[[nodiscard]] bool rayIntersectsTriangle(const PickingRay &ray,
                                         const glm::vec3 &a,
                                         const glm::vec3 &b,
                                         const glm::vec3 &c,
                                         float maxDistance,
                                         float &distance);

} // namespace Cthulhu::Editor
