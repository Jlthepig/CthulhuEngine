#include "pickingMath.hpp"

#include <algorithm>
#include <cmath>

namespace Cthulhu::Editor
{

namespace
{
bool finiteVector(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace

bool rayIntersectsBounds(const PickingRay &ray, const glm::vec3 &minimum, const glm::vec3 &maximum, float maxDistance)
{
    if (!finiteVector(ray.origin) || !finiteVector(ray.direction) || !finiteVector(minimum) || !finiteVector(maximum) ||
        !std::isfinite(maxDistance) || maxDistance <= 0.0f || glm::dot(ray.direction, ray.direction) == 0.0f)
    {
        return false;
    }

    double entry = 0.0;
    double exit = maxDistance;

    for (int axis = 0; axis < 3; ++axis)
    {
        const double origin = ray.origin[axis];
        const double direction = ray.direction[axis];
        const double low = minimum[axis];
        const double high = maximum[axis];

        if (low > high)
            return false;

        if (direction == 0.0)
        {
            if (origin < low || origin > high)
                return false;

            continue;
        }

        double nearDistance = (low - origin) / direction;
        double farDistance = (high - origin) / direction;

        if (nearDistance > farDistance)
            std::swap(nearDistance, farDistance);

        entry = std::max(entry, nearDistance);
        exit = std::min(exit, farDistance);

        if (entry > exit)
            return false;
    }

    return true;
}

bool rayIntersectsTriangle(const PickingRay &ray,
                           const glm::vec3 &a,
                           const glm::vec3 &b,
                           const glm::vec3 &c,
                           float maxDistance,
                           float &distance)
{
    if (!finiteVector(ray.origin) || !finiteVector(ray.direction) || !finiteVector(a) || !finiteVector(b) ||
        !finiteVector(c) || !std::isfinite(maxDistance) || maxDistance <= 0.0f)
    {
        return false;
    }

    const glm::dvec3 origin(ray.origin);
    const glm::dvec3 direction(ray.direction);
    const glm::dvec3 vertex(a);
    const glm::dvec3 edge1 = glm::dvec3(b) - vertex;
    const glm::dvec3 edge2 = glm::dvec3(c) - vertex;

    const glm::dvec3 perpendicular = glm::cross(direction, edge2);
    const double determinant = glm::dot(edge1, perpendicular);

    if (determinant == 0.0)
        return false;

    const double inverseDeterminant = 1.0 / determinant;
    const glm::dvec3 offset = origin - vertex;

    const double u = glm::dot(offset, perpendicular) * inverseDeterminant;

    if (u < 0.0 || u > 1.0)
        return false;

    const glm::dvec3 crossOffset = glm::cross(offset, edge1);
    const double v = glm::dot(direction, crossOffset) * inverseDeterminant;

    if (v < 0.0 || u + v > 1.0)
        return false;

    const double hitDistance = glm::dot(edge2, crossOffset) * inverseDeterminant;

    if (!std::isfinite(hitDistance) || hitDistance < 0.0 || hitDistance > maxDistance)
    {
        return false;
    }

    distance = static_cast<float>(hitDistance);
    return true;
}
} // namespace Cthulhu::Editor
