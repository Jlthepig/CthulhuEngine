#include <algorithm>
#include <cmath>
#include <limits>

#include "engine.hpp"
#include "log_utils.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "transformUtils.hpp"
#include "viewportPanel.hpp"

namespace Cthulhu::Editor
{
bool ViewportPanel::beginScaleDrag(Engine &engine,
                                   const glm::mat4 &inverseViewProjection,
                                   const octogui::Rect &viewport)
{
    auto &scale = drag.scale;
    scale.uniform = drag.handle == GizmoHandle::Centre;
    if (!scale.uniform && drag.handle != GizmoHandle::X && drag.handle != GizmoHandle::Y &&
        drag.handle != GizmoHandle::Z)
    {
        return false;
    }
    if (scale.uniform)
    {
        if (!prepareDragParents(engine))
        {
            return false;
        }
        drag.viewport = viewport;
    }
    else
    {
        if (!beginLinearDrag(engine, inverseViewProjection, viewport))
        {
            return false;
        }
        scale.axis = drag.linear.u;
    }

    scale.minimumFactor = 0.0001;
    scale.maximumFactor = std::numeric_limits<double>::max();
    scale.offsets.assign(drag.values.size(), glm::dvec3(0.0));
    scale.localAxes.assign(drag.values.size(), -1);
    auto *scene = engine.getActiveScene();
    for (std::size_t i = 0; i < drag.values.size(); ++i)
    {
        if (drag.followsSelection[i])
        {
            continue;
        }
        const auto &value = drag.values[i];
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!std::isfinite(value.scale[axis]) || value.scale[axis] == 0.0f)
            {
                KalaHeaders::KalaLog::Log::Print("SCALE REQUIRES FINITE NONZERO STARTING SCALES",
                                                 "Gizmo",
                                                 KalaHeaders::KalaLog::LogType::LOG_WARNING);
                return false;
            }
        }
        const auto entity = scene->findEntity(value.id);
        if (!entity)
        {
            return false;
        }
        const auto world = Scene::getWorldTransform(*entity).matrix;
        scale.offsets[i] = glm::dvec3(world[3]) - glm::dvec3(drag.frame.pivot);

        if (!scale.uniform)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                const glm::dvec3 column(world[axis]);
                const double length = glm::length(column);
                if (!std::isfinite(length) || length <= 0.0)
                {
                    return false;
                }
                const double alignment = std::abs(glm::dot(column / length, scale.axis));
                if (alignment >= 1.0 - 1.0e-5)
                {
                    if (scale.localAxes[i] != -1)
                    {
                        return false;
                    }
                    scale.localAxes[i] = axis;
                }
                else if (alignment > 1.0e-5)
                {
                    KalaHeaders::KalaLog::Log::Print(
                        "AXIS SCALE WOULD REQUIRE SHEAR; USE ALIGNED LOCAL AXES OR UNIFORM SCALE",
                        "Gizmo",
                        KalaHeaders::KalaLog::LogType::LOG_WARNING);
                    return false;
                }
            }
            if (scale.localAxes[i] == -1)
            {
                return false;
            }
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            if (!scale.uniform && axis != scale.localAxes[i])
            {
                continue;
            }
            const double magnitude = std::abs(static_cast<double>(value.scale[axis]));
            // Preserve pre-existing tiny scales without allowing further shrinkage.
            scale.minimumFactor = std::max(scale.minimumFactor, std::min(0.000001, magnitude) / magnitude);
            scale.maximumFactor = std::min(scale.maximumFactor, std::numeric_limits<float>::max() / magnitude);
        }
    }
    return std::isfinite(scale.minimumFactor) && std::isfinite(scale.maximumFactor) &&
           scale.minimumFactor <= scale.maximumFactor;
}

bool ViewportPanel::updateScaleDrag(octogui::Context &ctx, Scribe::Session &session)
{
    const auto &scale = drag.scale;
    double factor = 1.0;
    if (scale.uniform)
    {
        factor += (static_cast<double>(drag.mouseStart.y) - ctx.input().mouse.position.y) /
                  gizmoSystem.style.axisLengthPixels;
    }
    else
    {
        const auto coordinates = linearCoordinates(ctx.input().mouse.position);
        if (!coordinates)
        {
            return true;
        }
        factor += (coordinates->x - drag.linear.start.x) / drag.frame.axisLengthWorld;
    }
    if (snapActive && std::isfinite(snapScaleValue) && snapScaleValue > 0.0f)
    {
        const double step = snapScaleValue;
        factor = 1.0 + std::round((factor - 1.0) / step) * step;
    }
    if (!std::isfinite(factor))
    {
        return false;
    }
    factor = std::clamp(factor, scale.minimumFactor, scale.maximumFactor);

    const auto start = session.transformStart();
    if (start.size() != drag.values.size() || start.size() != drag.inverseParents.size() ||
        start.size() != drag.followsSelection.size() || start.size() != scale.offsets.size() ||
        start.size() != scale.localAxes.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < start.size(); ++i)
    {
        drag.values[i] = start[i];
        if (drag.followsSelection[i] || factor == 1.0)
        {
            continue;
        }

        const glm::dvec3 worldDelta = scale.uniform
                                          ? scale.offsets[i] * (factor - 1.0)
                                          : scale.axis * (glm::dot(scale.offsets[i], scale.axis) * (factor - 1.0));
        const glm::dvec3 position = glm::dvec3(start[i].position) + drag.inverseParents[i] * worldDelta;
        glm::dvec3 scaled(start[i].scale);
        if (scale.uniform)
        {
            scaled *= factor;
        }
        else
        {
            const int axis = scale.localAxes[i];
            if (axis < 0 || axis >= 3)
            {
                return false;
            }
            scaled[axis] *= factor;
        }
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!std::isfinite(position[axis]) || !std::isfinite(scaled[axis]) ||
                std::abs(position[axis]) > std::numeric_limits<float>::max() ||
                std::abs(scaled[axis]) > std::numeric_limits<float>::max() || scaled[axis] == 0.0)
            {
                return false;
            }
        }
        drag.values[i].position = glm::vec3(position);
        drag.values[i].scale = glm::vec3(scaled);
    }

    const auto result = session.previewTransforms(drag.values);
    if (!result.ok())
    {
        KalaHeaders::KalaLog::Log::Print(result.error, "Gizmo", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return false;
    }
    return true;
}
} // namespace Cthulhu::Editor
