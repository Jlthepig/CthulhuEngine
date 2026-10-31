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

struct ArrowVertex
{
    glm::vec3 position;
    float brightness;
};

const std::vector<ArrowVertex> &moveArrowMesh()
{
    static const std::vector<ArrowVertex> mesh = [] {
        constexpr int segments = 16;
        constexpr float start = 0.12f;
        constexpr float shoulder = 0.76f;
        constexpr float shaftRadius = 0.018f;
        constexpr float headRadius = 0.065f;
        constexpr float fullTurn = 6.28318530718f;

        std::vector<ArrowVertex> result;
        result.reserve(segments * 15);

        const glm::vec3 light = glm::normalize(glm::vec3{-0.4f, 0.6f, 1.0f});

        const auto triangle = [&](const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c) {
            const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));

            const float brightness = 0.65f + 0.35f * glm::max(glm::dot(normal, light), 0.0f);

            result.push_back({a, brightness});
            result.push_back({b, brightness});
            result.push_back({c, brightness});
        };

        for (int segment = 0; segment < segments; ++segment)
        {
            const float angleA = fullTurn * static_cast<float>(segment) / segments;
            const float angleB = fullTurn * static_cast<float>(segment + 1) / segments;

            const glm::vec2 a{std::cos(angleA), std::sin(angleA)};
            const glm::vec2 b{std::cos(angleB), std::sin(angleB)};

            const glm::vec3 shaftA{a * shaftRadius, start};
            const glm::vec3 shaftB{b * shaftRadius, start};
            const glm::vec3 shaftTopA{a * shaftRadius, shoulder};
            const glm::vec3 shaftTopB{b * shaftRadius, shoulder};
            const glm::vec3 headA{a * headRadius, shoulder};
            const glm::vec3 headB{b * headRadius, shoulder};

            triangle(shaftA, shaftB, shaftTopA);
            triangle(shaftB, shaftTopB, shaftTopA);
            triangle(glm::vec3{0.0f, 0.0f, start}, shaftB, shaftA);

            triangle(headA, headB, glm::vec3{0.0f, 0.0f, 1.0f});
            triangle(glm::vec3{0.0f, 0.0f, shoulder}, headB, headA);
        }

        return result;
    }();

    return mesh;
}

} // namespace

void GizmoSystem::update(Engine &engine,
                         std::span<const Scene::EntityId> selection,
                         GizmoMode mode,
                         GizmoSpace space,
                         float viewportHeight)
{
    vertices.clear();
    frame = {};

    if (engine.isPlaying() || mode == GizmoMode::Select || selection.empty() || !std::isfinite(viewportHeight) ||
        viewportHeight <= 0.0f || !std::isfinite(style.axisLengthPixels) || style.axisLengthPixels <= 0.0f)
    {
        return;
    }

    auto *scene = engine.getActiveScene();
    auto *camera = engine.getCamera();
    if (!scene || !camera)
        return;

    glm::dvec3 positionSum(0.0);
    std::size_t count = 0;

    for (const Scene::EntityId id : selection)
    {
        const auto entity = scene->findEntity(id);
        if (!entity || !entity->has<Scene::TransformComponent>())
            continue;

        const auto world = Scene::getWorldTransform(*entity);
        const glm::vec3 position(world.matrix[3]);

        if (!finiteVector(position))
            return;

        if (count == 0 && space == GizmoSpace::Local)
            frame.basis = glm::mat3_cast(world.rotation);

        positionSum += glm::dvec3(position);
        ++count;
    }

    if (count == 0)
        return;

    frame.pivot = glm::vec3(positionSum / static_cast<double>(count));

    const glm::vec4 viewPosition = camera->getViewMatrix() * glm::vec4(frame.pivot, 1.0f);
    const float depth = -viewPosition.z;

    const auto &config = engine.getRenderer().getConfig();
    if (!std::isfinite(depth) || depth <= config.nearPlane || depth >= config.farPlane)
    {
        return;
    }

    const glm::mat4 projection =
        glm::perspective(glm::radians(camera->getFov()), 1.0f, config.nearPlane, config.farPlane);

    const float projectionY = std::abs(projection[1][1]);
    if (!std::isfinite(projectionY) || projectionY <= 0.0f)
        return;

    frame.axisLengthWorld = (2.0f * depth / projectionY) * (style.axisLengthPixels / viewportHeight);

    frame.visible = finiteVector(frame.pivot) && finiteVector(frame.basis[0]) && finiteVector(frame.basis[1]) &&
                    finiteVector(frame.basis[2]) && std::isfinite(frame.axisLengthWorld) &&
                    frame.axisLengthWorld > 0.0f;

    if (!frame.visible || mode != GizmoMode::Move)
        return;

    const auto &arrow = moveArrowMesh();
    vertices.reserve(arrow.size() * 3);

    for (int axis = 0; axis < 3; ++axis)
    {
        const glm::vec3 side = frame.basis[(axis + 1) % 3];
        const glm::vec3 up = frame.basis[(axis + 2) % 3];
        const glm::vec3 forward = frame.basis[axis];

        for (const auto &vertex : arrow)
        {
            const glm::vec3 offset = side * vertex.position.x + up * vertex.position.y + forward * vertex.position.z;

            vertices.push_back({frame.pivot + offset * frame.axisLengthWorld,
                                glm::vec4(style.axisColors[axis] * vertex.brightness, 1.0f)});
        }
    }
}
} // namespace Cthulhu::Editor
