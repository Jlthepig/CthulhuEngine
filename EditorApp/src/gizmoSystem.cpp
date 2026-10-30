#include <cmath>

#include <gtc/matrix_transform.hpp>

#include "gizmoSystem.hpp"

#include "engine.hpp"
#include "scene.hpp"
#include "transformUtils.hpp"

namespace Cthulhu::Editor
{
namespace
{
bool finiteVector(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

void GizmoSystem::update(Engine &engine, std::span<const Scene::EntityId> selection, GizmoMode mode, GizmoSpace space, float viewportHeight)
{
    frame = {};

    if (engine.isPlaying() || mode == GizmoMode::Select ||
        selection.empty() ||
        !std::isfinite(viewportHeight) || viewportHeight <= 0.0f ||
        !std::isfinite(style.axisLengthPixels) ||
        style.axisLengthPixels <= 0.0f)
    {
        return;
    }

    auto *scene = engine.getActiveScene();
    auto *camera = engine.getCamera();
    if (!scene || !camera) return;

    glm::dvec3 positionSum(0.0);
    std::size_t count = 0;

    for (const Scene::EntityId id : selection)
    {
        const auto entity = scene->findEntity(id);
        if (!entity || !entity->has<Scene::TransformComponent>()) continue;

        const auto world = Scene::getWorldTransform(*entity);
        const glm::vec3 position(world.matrix[3]);

        if (!finiteVector(position)) return;

        if (count == 0 && space == GizmoSpace::Local) frame.basis = glm::mat3_cast(world.rotation);

        positionSum += glm::dvec3(position);
        ++count;
    }

    if (count == 0) return;

    frame.pivot = glm::vec3(positionSum / static_cast<double>(count));

    const glm::vec4 viewPosition = camera->getViewMatrix() * glm::vec4(frame.pivot, 1.0f);
    const float depth = -viewPosition.z;

    const auto &config = engine.getRenderer().getConfig();
    if (!std::isfinite(depth) || depth <= config.nearPlane || depth >= config.farPlane)
    {
        return;
    }

    const glm::mat4 projection = glm::perspective(glm::radians(camera->getFov()), 1.0f, config.nearPlane, config.farPlane);

    const float projectionY = std::abs(projection[1][1]);
    if (!std::isfinite(projectionY) || projectionY <= 0.0f) return;

    frame.axisLengthWorld = (2.0f * depth / projectionY) * (style.axisLengthPixels / viewportHeight);

    frame.visible =
        finiteVector(frame.pivot) &&
        finiteVector(frame.basis[0]) &&
        finiteVector(frame.basis[1]) &&
        finiteVector(frame.basis[2]) &&
        std::isfinite(frame.axisLengthWorld) &&
        frame.axisLengthWorld > 0.0f;
}
}