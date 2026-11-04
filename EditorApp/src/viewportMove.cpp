#include <cmath>
#include <limits>

#include <gtc/matrix_transform.hpp>

#include "engine.hpp"
#include "log_utils.hpp"
#include "session.hpp"
#include "viewportPanel.hpp"

namespace Cthulhu::Editor
{
std::optional<glm::dvec2> ViewportPanel::linearCoordinates(octogui::Vec2 point) const
{
    const auto &move = drag.linear;
    if (move.vertical)
    {
        return glm::dvec2{(static_cast<double>(drag.mouseStart.y) - point.y) * move.unitsPerPixel, 0.0};
    }

    const auto &rect = drag.viewport;
    const double x = 2.0 * (static_cast<double>(point.x) - rect.x) / rect.w - 1.0;
    const double y = 1.0 - 2.0 * (static_cast<double>(point.y) - rect.y) / rect.h;
    const glm::dmat4 inverse(move.inverseViewProjection);

    glm::dvec4 nearPoint = inverse * glm::dvec4{x, y, -1.0, 1.0};
    glm::dvec4 farPoint = inverse * glm::dvec4{x, y, 1.0, 1.0};
    if (!std::isfinite(nearPoint.w) || !std::isfinite(farPoint.w) ||
        nearPoint.w == 0.0 || farPoint.w == 0.0)
    {
        return std::nullopt;
    }

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::dvec3 segment(farPoint - nearPoint);
    const double length = glm::length(segment);
    if (!std::isfinite(length) || length <= 0.0)
    {
        return std::nullopt;
    }

    const glm::dvec3 direction = segment / length;
    const double denominator = glm::dot(direction, move.normal);
    if (!std::isfinite(denominator) || std::abs(denominator) < 0.05)
    {
        return std::nullopt;
    }

    const glm::dvec3 origin(nearPoint);
    const double distance = glm::dot(glm::dvec3(drag.frame.pivot) - origin, move.normal) / denominator;
    if (!std::isfinite(distance) || distance < 0.0)
    {
        return std::nullopt;
    }

    const glm::dvec3 offset = origin + direction * distance - glm::dvec3(drag.frame.pivot);
    const glm::dvec2 coordinates{glm::dot(offset, move.u), glm::dot(offset, move.v)};
    if (!std::isfinite(coordinates.x) || !std::isfinite(coordinates.y))
    {
        return std::nullopt;
    }

    return coordinates;
}

bool ViewportPanel::beginLinearDrag(Engine &engine,const glm::mat4 &inverseViewProjection, const octogui::Rect &viewport)
{
    if (!prepareDragParents(engine))
    {
        return false;
    }

    auto &move = drag.linear;
    move.u = {};
    move.v = {};
    move.normal = {};
    move.start = {};
    move.vertical = false;
    move.inverseViewProjection = inverseViewProjection;
    drag.viewport = viewport;
    move.unitsPerPixel = static_cast<double>(drag.frame.axisLengthWorld) / gizmoSystem.style.axisLengthPixels;

    const auto &basis = drag.frame.basis;
    const bool axis = drag.handle == GizmoHandle::X || drag.handle == GizmoHandle::Y ||
                      drag.handle == GizmoHandle::Z;

    switch (drag.handle)
    {
        case GizmoHandle::X:
            move.u = basis[0];
            break;
        case GizmoHandle::Y:
            move.u = basis[1];
            break;
        case GizmoHandle::Z:
            move.u = basis[2];
            break;
        case GizmoHandle::XY:
            move.u = basis[0];
            move.v = basis[1];
            break;
        case GizmoHandle::YZ:
            move.u = basis[1];
            move.v = basis[2];
            break;
        case GizmoHandle::ZX:
            move.u = basis[2];
            move.v = basis[0];
            break;
        case GizmoHandle::Centre:
            move.u = drag.cameraBasis[0];
            move.v = drag.cameraBasis[1];
            break;
        default:
            return false;
    }

    if (axis)
    {
        const glm::dvec3 forward = -glm::dvec3(drag.cameraBasis[2]);
        move.normal = forward - move.u * glm::dot(forward, move.u);
        const double length = glm::length(move.normal);
        move.vertical = length < 0.1;
        if (!move.vertical)
        {
            move.normal /= length;
        }
    }
    else
    {
        move.normal = glm::normalize(glm::cross(move.u, move.v));
    }

    auto start = linearCoordinates(drag.mouseStart);
    if (!start && axis)
    {
        move.vertical = true;
        start = linearCoordinates(drag.mouseStart);
    }

    if (!start)
    {
        KalaHeaders::KalaLog::Log::Print(
            "DRAG PLANE IS EDGE-ON TO THE CAMERA", "Gizmo", KalaHeaders::KalaLog::LogType::LOG_WARNING);
        return false;
    }

    move.start = *start;
    return true;
}

bool ViewportPanel::updateMoveDrag(octogui::Context &ctx, Scribe::Session &session)
{
    const auto coordinates = linearCoordinates(ctx.input().mouse.position);
    if (!coordinates)
    {
        return true;
    }

    glm::dvec2 delta = *coordinates - drag.linear.start;
    if (snapActive && std::isfinite(snapMoveValue) && snapMoveValue > 0.0f)
    {
        const double step = snapMoveValue;
        delta.x = std::round(delta.x / step) * step;
        delta.y = std::round(delta.y / step) * step;
    }

    const glm::dvec3 worldDelta = drag.linear.u * delta.x + drag.linear.v * delta.y;
    const auto start = session.transformStart();
    if (start.size() != drag.values.size() ||
        start.size() != drag.inverseParents.size() ||
        start.size() != drag.followsSelection.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < start.size(); ++i)
    {
        if (drag.followsSelection[i])
        {
            drag.values[i].position = start[i].position;
            continue;
        }

        const glm::dvec3 localDelta = drag.inverseParents[i] * worldDelta;
        const glm::dvec3 position = glm::dvec3(start[i].position) + localDelta;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!std::isfinite(position[axis]) ||
                std::abs(position[axis]) > std::numeric_limits<float>::max())
            {
                return false;
            }
        }
        drag.values[i].position = glm::vec3(position);
    }

    const auto result = session.previewTransforms(drag.values);
    if (!result.ok())
    {
        KalaHeaders::KalaLog::Log::Print(
            result.error, "Gizmo", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return false;
    }
    drag.translation = worldDelta;

    return true;
}
} // namespace Cthulhu::Editor
