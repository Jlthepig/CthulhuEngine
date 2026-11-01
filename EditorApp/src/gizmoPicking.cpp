#include <algorithm>
#include <cmath>
#include <limits>

#include "gizmoSystem.hpp"

#include "pickingMath.hpp"


namespace
{
double screenEdgeDistanceSquared(glm::dvec4 a, glm::dvec4 b, const glm::vec2 &viewportSize, const glm::vec2 &point)
{
    constexpr double noHit = std::numeric_limits<double>::infinity();

    for (int component = 0; component < 4; ++component)
    {
        if (!std::isfinite(a[component]) || !std::isfinite(b[component]))
        {
            return noHit;
        }
    }

    for (int plane = 0; plane < 2; ++plane)
    {
        const double da = a.w + (plane == 0 ? a.z : -a.z);
        const double db = b.w + (plane == 0 ? b.z : -b.z);

        if (da < 0.0 && db < 0.0)
        {
            return noHit;
        }

        if ((da < 0.0) != (db < 0.0))
        {
            const glm::dvec4 intersection = a + (b - a) * (da / (da - db));

            if (da < 0.0)
            {
                a = intersection;
            }
            else
            {
                b = intersection;
            }
        }
    }

    if (a.w <= 0.0 || b.w <= 0.0)
    {
        return noHit;
    }

    const glm::dvec2 start{(a.x / a.w + 1.0) * 0.5 * viewportSize.x, (1.0 - a.y / a.w) * 0.5 * viewportSize.y};
    const glm::dvec2 end{(b.x / b.w + 1.0) * 0.5 * viewportSize.x, (1.0 - b.y / b.w) * 0.5 * viewportSize.y};

    const glm::dvec2 edge = end - start;
    const glm::dvec2 offset = glm::dvec2(point) - start;
    const double lengthSquared = glm::dot(edge, edge);
    const double t = lengthSquared > 0.0 ? std::clamp(glm::dot(offset, edge) / lengthSquared, 0.0, 1.0) : 0.0;

    const glm::dvec2 delta = offset - edge * t;
    return glm::dot(delta, delta);
}
} // namespace

namespace Cthulhu::Editor
{
GizmoHandle GizmoSystem::pick(const PickingRay &ray, float maxDistance, const glm::mat4 &viewProjection, const glm::vec2 &viewportSize, 
                                const glm::vec2 mousePosition, GizmoHandle previous) const
{
    GizmoHandle result = GizmoHandle::None;
    float nearestDistance = maxDistance;

    const glm::dmat4 projection(viewProjection);
    const double paddingSquared = static_cast<double>(style.hitPaddingPixels) * style.hitPaddingPixels;
    double bestDistanceSquared = paddingSquared;
    double previousDistanceSquared = std::numeric_limits<double>::infinity();
    GizmoHandle padded = GizmoHandle::None;

    for (const GizmoPart &part : parts)
    {
        if (part.handle == GizmoHandle::None ||
            part.firstVertex > vertices.size() ||
            part.vertexCount > vertices.size() - part.firstVertex ||
            part.firstVertex % 3 != 0 || part.vertexCount % 3 != 0)
        {
            continue;
        }

        const std::size_t end = part.firstVertex + part.vertexCount;
        for (std::size_t first = part.firstVertex; first < end; first += 3)
        {
            float distance = 0.0f;
            if (rayIntersectsTriangle(ray,vertices[first].position, vertices[first + 1].position, vertices[first + 2].position, 
                nearestDistance, distance))
            {
                if (result == GizmoHandle::None || distance < nearestDistance)
                {
                    nearestDistance = distance;
                    result = part.handle;
                }
            }

            if (result != GizmoHandle::None || style.hitPaddingPixels <= 0.0f)
            {
                continue;
            }

            const glm::dvec4 clip[3]{
                projection * glm::dvec4(vertices[first].position, 1.0),
                projection * glm::dvec4(vertices[first + 1].position, 1.0),
                projection * glm::dvec4(vertices[first + 2].position, 1.0)};

            double distanceSquared = std::numeric_limits<double>::infinity();

            for (int edge = 0; edge < 3; ++edge)
            {
                distanceSquared = std::min(distanceSquared, screenEdgeDistanceSquared(clip[edge], clip[(edge + 1) % 3], viewportSize, mousePosition));
            }

            if (part.handle == previous)
            {
                previousDistanceSquared = std::min(previousDistanceSquared, distanceSquared);
            }

            if (distanceSquared <= bestDistanceSquared && (padded == GizmoHandle::None || distanceSquared < bestDistanceSquared))
            {
                bestDistanceSquared = distanceSquared;
                padded = part.handle;
            }

        }
    }

    if (result != GizmoHandle::None)
    {
        return result;
    }

    if (padded != GizmoHandle::None && previousDistanceSquared <= paddingSquared &&
        std::sqrt(previousDistanceSquared) <= std::sqrt(bestDistanceSquared) + style.hoverBiasPixels)
    {
        return previous;
    }

    return padded;
}
} // namespace Cthulhu::Editor