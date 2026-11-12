#include <algorithm>
#include <cmath>
#include <limits>
#include <gtx/euler_angles.hpp>

#include "engine.hpp"
#include "log_utils.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "transformUtils.hpp"
#include "viewportPanel.hpp"

namespace Cthulhu::Editor
{

namespace
{
bool fitsFloat(const glm::dvec3 &value)
{
    for (int axis = 0; axis < 3; ++axis)
    {
        if (!std::isfinite(value[axis]) ||
            std::abs(value[axis]) > std::numeric_limits<float>::max())
        {
            return false;
        }
    }
    return true;
}

bool setLocalLinear(const glm::dmat3 &linear, Scribe::TransformValue &value)
{
    constexpr double minimumRelativeAxis = 1.0e-10;
    constexpr double reconstructionTolerance = 2.0e-5;

    for (int column = 0; column < 3; ++column)
    {
        if (!fitsFloat(linear[column]))
        {
            return false;
        }
    }

    const double lengthX = glm::length(linear[0]);
    const double lengthY = glm::length(linear[1]);
    const double lengthZ = glm::length(linear[2]);
    if (lengthX == 0.0 || lengthY == 0.0 || lengthZ == 0.0)
    {
        return false;
    }

    const glm::dvec3 x = linear[0] / lengthX * std::copysign(1.0, value.scale.x);

    glm::dvec3 y = linear[1] - x * glm::dot(x, linear[1]);
    y -= x * glm::dot(x, y);
    const double lengthOrthogonalY = glm::length(y);
    if (lengthOrthogonalY <= lengthY * minimumRelativeAxis)
    {
        return false;
    }
    y *= std::copysign(1.0, value.scale.y) / lengthOrthogonalY;

    const glm::dvec3 z = glm::normalize(glm::cross(x, y));
    y = glm::cross(z, x);

    const glm::dvec3 signedScale{glm::dot(x, linear[0]), glm::dot(y, linear[1]), glm::dot(z, linear[2])
    };
    if (std::abs(signedScale.z) <= lengthZ * minimumRelativeAxis || std::signbit(signedScale.z) != std::signbit(value.scale.z) || !fitsFloat(signedScale))
    {
        return false;
    }

    const glm::dvec3 shear{ glm::dot(x, linear[1]) / signedScale.y, glm::dot(x, linear[2]) / signedScale.z, glm::dot(y, linear[2]) / signedScale.z
    };
    glm::dvec3 euler{};
    glm::extractEulerAngleXYZ(glm::dmat4(glm::dmat3(x, y, z)), euler.x, euler.y, euler.z);

    if (!fitsFloat(shear) || !fitsFloat(euler))
    {
        return false;
    }

    const Scene::TransformComponent candidate{
        .rotation = glm::vec3(euler),
        .scale = glm::vec3(signedScale),
        .shear = glm::vec3(shear)
    };
    constexpr float kMinAxisScale = 1e-6f;
    if (std::abs(candidate.scale.x) < kMinAxisScale || std::abs(candidate.scale.y) < kMinAxisScale ||
        std::abs(candidate.scale.z) < kMinAxisScale)
    {
        return false;
    }

    const glm::mat4 rebuilt = Scene::composeLocalMatrix(candidate, Scene::localRotationMatrix(candidate.rotation));
    for (int column = 0; column < 3; ++column)
    {
        const glm::dvec3 rebuiltColumn(rebuilt[column]);
        const double error = glm::length(rebuiltColumn - linear[column]);
        if (!std::isfinite(error) || error > glm::length(linear[column]) * reconstructionTolerance)
        {
            return false;
        }
    }

    value.rotation = candidate.rotation;
    value.scale = candidate.scale;
    value.shear = candidate.shear;
    return true;
}
} // namespace

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
        scale.axis = glm::normalize(drag.linear.u);
    }

    scale.minimumFactor = 0.0001;
    scale.maximumFactor = std::numeric_limits<double>::max();
    scale.roots.resize(drag.values.size());
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
            const double magnitude = std::abs(static_cast<double>(value.scale[axis]));
            if (!std::isfinite(magnitude) || magnitude == 0.0)
            {
                KalaHeaders::KalaLog::Log::Print("SCALE REQUIRES FINITE NONZERO STARTING SCALES",
                                                 "Gizmo",
                                                 KalaHeaders::KalaLog::LogType::LOG_WARNING);
                return false;
            }

            if (scale.uniform)
            {
                scale.minimumFactor = std::max(scale.minimumFactor, std::min(0.000001, magnitude) / magnitude);
                scale.maximumFactor = std::min(scale.maximumFactor, std::numeric_limits<float>::max() / magnitude);
            }
        }

        const auto entity = scene->findEntity(value.id);
        if (!entity)
        {
            return false;
        }
        const glm::mat4 world = Scene::getWorldTransform(*entity).matrix;
        for (int column = 0; column < 4; ++column)
        {
            if (!fitsFloat(glm::dvec3(world[column])))
            {
                return false;
            }
        }

        scale.roots[i] = {
            glm::dmat3(world),
            glm::dvec3(world[3]) - glm::dvec3(drag.frame.pivot)
        };
    }

    return scale.minimumFactor <= scale.maximumFactor;
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
        start.size() != drag.followsSelection.size() || start.size() != scale.roots.size())
    {
        return false;
    }

    const double delta = factor - 1.0;
    const glm::dmat3 stretch = scale.uniform
                                   ? glm::dmat3(factor)
                                   : glm::dmat3(1.0) + delta * glm::outerProduct(scale.axis, scale.axis);

    for (std::size_t i = 0; i < start.size(); ++i)
    {
        auto &value = drag.values[i];
        value = start[i];
        if (drag.followsSelection[i] || factor == 1.0)
        {
            continue;
        }

        const auto &root = scale.roots[i];
        const auto &inverseParent = drag.inverseParents[i];
        const glm::dvec3 worldDelta = scale.uniform
                                          ? root.offset * delta
                                          : scale.axis * (glm::dot(root.offset, scale.axis) * delta);
        const glm::dvec3 worldPosition = glm::dvec3(drag.frame.pivot) + root.offset + worldDelta;
        const glm::dvec3 position = glm::dvec3(value.position) + inverseParent * worldDelta;
        const glm::dmat3 worldLinear = stretch * root.worldLinear;

        if (!fitsFloat(position) || !fitsFloat(worldPosition))
        {
            return true;
        }
        for (int column = 0; column < 3; ++column)
        {
            if (!fitsFloat(worldLinear[column]))
            {
                return true;
            }
        }

        if (scale.uniform)
        {
            const glm::dvec3 scaled = glm::dvec3(value.scale) * factor;
            if (!fitsFloat(scaled))
            {
                return true;
            }
            const glm::vec3 storedScale(scaled);
            if (storedScale.x == 0.0f || storedScale.y == 0.0f || storedScale.z == 0.0f)
            {
                return true;
            }
            value.scale = storedScale;
        }
        else if (!setLocalLinear(inverseParent * worldLinear, value))
        {
            return true;
        }

        value.position = glm::vec3(position);
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
