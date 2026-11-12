#include <algorithm>
#include <cmath>
#include <limits>

#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
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
        if (!std::isfinite(value[axis]) || std::abs(value[axis]) > std::numeric_limits<float>::max())
        {
            return false;
        }
    }
    return true;
}
} // namespace

std::optional<double> ViewportPanel::rotateAngle(octogui::Vec2 point) const
{
    const auto &rotate = drag.rotate;
    const auto &rect = drag.viewport;
    const double x = 2.0 * (static_cast<double>(point.x) - rect.x) / rect.w - 1.0;
    const double y = 1.0 - 2.0 * (static_cast<double>(point.y) - rect.y) / rect.h;
    const glm::dmat4 inverse(rotate.inverseViewProjection);
    glm::dvec4 nearPoint = inverse * glm::dvec4{x, y, -1.0, 1.0};
    glm::dvec4 farPoint = inverse * glm::dvec4{x, y, 1.0, 1.0};
    if (!std::isfinite(nearPoint.w) || !std::isfinite(farPoint.w) || nearPoint.w == 0.0 || farPoint.w == 0.0)
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
    const double denominator = glm::dot(direction, rotate.axis);
    if (!std::isfinite(denominator) || std::abs(denominator) < 0.1)
    {
        return std::nullopt;
    }
    const glm::dvec3 origin(nearPoint);
    const double distance = glm::dot(glm::dvec3(drag.frame.pivot) - origin, rotate.axis) / denominator;
    if (!std::isfinite(distance) || distance < 0.0)
    {
        return std::nullopt;
    }
    const glm::dvec3 radial = origin + direction * distance - glm::dvec3(drag.frame.pivot);
    const double u = glm::dot(radial, rotate.u);
    const double v = glm::dot(radial, rotate.v);
    const double deadZone = drag.frame.axisLengthWorld * gizmoSystem.style.rotationRadius * 0.1;
    if (!std::isfinite(u) || !std::isfinite(v) || u * u + v * v <= deadZone * deadZone)
    {
        return std::nullopt;
    }
    return std::atan2(v, u);
}

bool ViewportPanel::beginRotateDrag(Engine &engine,
                                    const glm::mat4 &inverseViewProjection,
                                    const octogui::Rect &viewport)
{
    if (!prepareDragParents(engine))
    {
        return false;
    }
    auto &rotate = drag.rotate;
    rotate.inverseViewProjection = inverseViewProjection;
    drag.viewport = viewport;
    rotate.angle = 0.0;
    rotate.previousAngle = 0.0;
    rotate.startAngle = 0.0;
    rotate.appliedAngle = 0.0;
    rotate.tracking = false;
    const double radius = gizmoSystem.style.axisLengthPixels * gizmoSystem.style.rotationRadius;
    if (!std::isfinite(radius) || radius <= 0.0)
    {
        return false;
    }
    rotate.radiansPerPixel = 1.0 / radius;

    int axis = 0;
    switch (drag.handle)
    {
        case GizmoHandle::X:
            axis = 0;
            break;
        case GizmoHandle::Y:
            axis = 1;
            break;
        case GizmoHandle::Z:
            axis = 2;
            break;
        default:
            return false;
    }
    rotate.axis = drag.frame.basis[axis];
    rotate.u = drag.frame.basis[(axis + 1) % 3];
    rotate.v = drag.frame.basis[(axis + 2) % 3];

    rotate.offsets.assign(drag.values.size(), glm::dvec3(0.0));
    rotate.initialRotations.assign(drag.values.size(), glm::dmat3(1.0));
    rotate.inverseFrames.assign(drag.values.size(), glm::dmat3(1.0));
    auto *scene = engine.getActiveScene();
    for (std::size_t i = 0; i < drag.values.size(); ++i)
    {
        if (drag.followsSelection[i])
        {
            continue;
        }

        const auto &inverseParent = drag.inverseParents[i];
        const double scale = glm::length(inverseParent[0]);
        if (!std::isfinite(scale) || scale <= 0.0)
        {
            return false;
        }
        const glm::dmat3 frame = inverseParent / scale;
        for (int column = 0; column < 3; ++column)
        {
            for (int row = 0; row < 3; ++row)
            {
                const double expected = column == row ? 1.0 : 0.0;
                const double value = glm::dot(frame[column], frame[row]);
                if (!std::isfinite(value) || std::abs(value - expected) > 1.0e-5)
                {
                    KalaHeaders::KalaLog::Log::Print("ROTATE REQUIRES A PARENT WITHOUT NONUNIFORM SCALE OR SHEAR",
                                                     "Gizmo",
                                                     KalaHeaders::KalaLog::LogType::LOG_WARNING);
                    return false;
                }
            }
        }
        rotate.inverseFrames[i] = frame;
        const auto entity = scene->findEntity(drag.values[i].id);
        if (!entity)
        {
            return false;
        }
        const glm::vec3 euler = drag.values[i].rotation;
        rotate.initialRotations[i] = glm::dmat3(glm::eulerAngleXYZ(static_cast<double>(euler.x),
                                                                   static_cast<double>(euler.y),
                                                                   static_cast<double>(euler.z)));
        rotate.offsets[i] = glm::dvec3(Scene::getWorldTransform(*entity).matrix[3]) - glm::dvec3(drag.frame.pivot);
    }

    const auto angle = rotateAngle(drag.mouseStart);
    rotate.vertical = !angle;
    if (angle)
    {
        rotate.previousAngle = *angle;
        rotate.startAngle = *angle;
        rotate.tracking = true;
    }
    return true;
}

bool ViewportPanel::updateRotateDrag(octogui::Context &ctx, Scribe::Session &session)
{
    auto &rotate = drag.rotate;
    if (rotate.vertical)
    {
        rotate.angle = (static_cast<double>(drag.mouseStart.y) - ctx.input().mouse.position.y) * rotate.radiansPerPixel;
    }
    else
    {
        const auto angle = rotateAngle(ctx.input().mouse.position);
        if (!angle)
        {
            rotate.tracking = false;
            return true;
        }
        if (rotate.tracking)
        {
            rotate.angle += std::remainder(*angle - rotate.previousAngle, glm::two_pi<double>());
        }
        rotate.previousAngle = *angle;
        rotate.tracking = true;
    }

    double angle = rotate.angle;
    if (snapActive && std::isfinite(snapRotateValue) && snapRotateValue > 0.0f)
    {
        const double step = glm::radians(static_cast<double>(snapRotateValue));
        angle = std::round(angle / step) * step;
    }
    if (!std::isfinite(angle))
    {
        return false;
    }
    const double appliedAngle = angle;
    angle = std::remainder(angle, glm::two_pi<double>());
    const auto start = session.transformStart();
    if (start.size() != drag.values.size() || start.size() != drag.inverseParents.size() ||
        start.size() != drag.followsSelection.size() || start.size() != rotate.offsets.size() ||
        start.size() != rotate.initialRotations.size() || start.size() != rotate.inverseFrames.size())
    {
        return false;
    }

    const glm::dmat3 worldRotation(glm::rotate(glm::dmat4(1.0), angle, rotate.axis));
    for (std::size_t i = 0; i < start.size(); ++i)
    {
        if (drag.followsSelection[i] || std::abs(angle) < 1.0e-12)
        {
            drag.values[i] = start[i];
            continue;
        }
        const glm::dvec3 worldDelta = worldRotation * rotate.offsets[i] - rotate.offsets[i];
        const glm::dvec3 position = glm::dvec3(start[i].position) + drag.inverseParents[i] * worldDelta;
        const auto &frame = rotate.inverseFrames[i];
        const glm::dmat3 localRotation = frame * worldRotation * glm::transpose(frame) * rotate.initialRotations[i];
        glm::dvec3 euler;
        glm::extractEulerAngleXYZ(glm::dmat4(localRotation), euler.x, euler.y, euler.z);

        for (int axis = 0; axis < 3; ++axis)
        {
            euler[axis] +=
                std::round((static_cast<double>(drag.values[i].rotation[axis]) - euler[axis]) / glm::two_pi<double>()) *
                glm::two_pi<double>();
        }
        if (!fitsFloat(position) || !fitsFloat(euler))
        {
            return false;
        }
        drag.values[i].position = glm::vec3(position);
        drag.values[i].rotation = glm::vec3(euler);
    }

    const auto result = session.previewTransforms(drag.values);
    if (!result.ok())
    {
        KalaHeaders::KalaLog::Log::Print(result.error, "Gizmo", KalaHeaders::KalaLog::LogType::LOG_ERROR);
        return false;
    }
    rotate.appliedAngle = appliedAngle;
    return true;
}
} // namespace Cthulhu::Editor
