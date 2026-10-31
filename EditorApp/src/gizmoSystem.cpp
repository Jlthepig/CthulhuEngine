#include <cmath>
#include <algorithm>
#include <array>

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

struct RingVertex
{
    glm::vec2 radial;
    glm::vec2 tube;
};

const std::vector<RingVertex> &rotationRingMesh()
{
    static const std::vector<RingVertex> mesh = []
    {
        constexpr int segments = 64;
        constexpr int sides = 6;
        constexpr float fullTurn = 6.28318530718f;

        std::vector<RingVertex> result;
        result.reserve(segments * sides * 6);

        const auto vertex = [&](int segment, int side)
        {
            const float angle = fullTurn * static_cast<float>(segment % segments) / segments;
            const float tubeAngle = fullTurn * static_cast<float>(side % sides) / sides;

            return RingVertex{{std::cos(angle), std::sin(angle)},{std::cos(tubeAngle), std::sin(tubeAngle)}};
        };

        for (int segment = 0; segment < segments; ++segment)
        {
            for (int side = 0; side < sides; ++side)
            {
                const auto a = vertex(segment, side);
                const auto b = vertex(segment + 1, side);
                const auto c = vertex(segment + 1, side + 1);
                const auto d = vertex(segment, side + 1);

                result.insert(result.end(), {a, b, c, a, c, d});
            }
        }

        return result;
    }();

    return mesh;
}

void appendRotationRings(const GizmoFrame &frame, const GizmoStyle &style,const Scene::Camera &camera, std::vector<Rendering::OverlayVertex> &vertices)
{
    const float radius = frame.axisLengthWorld * style.rotationRadius;
    const float tubeRadius = frame.axisLengthWorld * (style.rotationThicknessPixels * 0.5f) / style.axisLengthPixels;

    if (!std::isfinite(radius) || !std::isfinite(tubeRadius) || radius <= 0.0f || tubeRadius <= 0.0f || tubeRadius >= radius)
    {
        return;
    }

    const glm::mat4 inverseView = glm::inverse(camera.getViewMatrix());
    const glm::vec3 light = glm::normalize(glm::vec3(inverseView[0]) * 0.35f + glm::vec3(inverseView[1]) * 0.55f + glm::vec3(inverseView[2]));

    const glm::vec3 viewDirection = camera.getFront();
    const float rearBrightness = std::clamp(style.rotationRearBrightness, 0.0f, 1.0f);

    const auto &mesh = rotationRingMesh();
    vertices.reserve(vertices.size() + mesh.size() * 3);

    for (int axis = 0; axis < 3; ++axis)
    {
        const glm::vec3 u = frame.basis[(axis + 1) % 3];
        const glm::vec3 v = frame.basis[(axis + 2) % 3];
        const glm::vec3 normal = frame.basis[axis];

        const glm::vec2 planeView{ glm::dot(u, viewDirection), glm::dot(v, viewDirection)};
        const float planeViewLength = glm::length(planeView);

        for (const auto &vertex : mesh)
        {
            const glm::vec3 radial = u * vertex.radial.x + v * vertex.radial.y;
            const glm::vec3 surfaceNormal = radial * vertex.tube.x + normal * vertex.tube.y;

            const glm::vec3 position = frame.pivot + radial * (radius + tubeRadius * vertex.tube.x) + normal * (tubeRadius * vertex.tube.y);

            float rear = 0.0f;
            if (planeViewLength > 0.0001f)
            {
                rear = glm::smoothstep(0.0f, planeViewLength * 0.20f, glm::dot(radial, viewDirection));
            }

            const float lighting = 0.65f + 0.35f * glm::max(glm::dot(surfaceNormal, light), 0.0f);
            const float brightness = lighting * glm::mix(1.0f, rearBrightness, rear);

            vertices.push_back({position, glm::vec4(style.axisColors[axis] * brightness, 1.0f)});
        }
    }
}

void appendQuad(std::vector<Rendering::OverlayVertex> &vertices, const glm::vec3 &origin, const glm::vec3 &u, const glm::vec3 &v, const glm::vec4 &color)
{
    vertices.insert(vertices.end(), {
        {origin, color},
        {origin + u, color},
        {origin + u + v, color},
        {origin, color},
        {origin + u + v, color},
        {origin + v, color}});
}

void appendBorder(std::vector<Rendering::OverlayVertex> &vertices, const glm::vec3 &origin, const glm::vec3 &u, const glm::vec3 &v, float inset, const glm::vec4 &color)
{
    appendQuad(vertices, origin, u, v * inset, color);
    appendQuad(vertices, origin + v * (1.0f - inset), u, v * inset, color);

    const glm::vec3 innerV = v * (1.0f - 2.0f * inset);
    appendQuad(vertices, origin + v * inset, u * inset, innerV, color);
    appendQuad(vertices, origin + u * (1.0f - inset) + v * inset, u * inset, innerV, color);
}

} // namespace

void GizmoSystem::update(Engine &engine, std::span<const Scene::EntityId> selection, GizmoMode mode, GizmoSpace space, float viewportHeight)
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
    if (!scene || !camera) return;

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

    if (count == 0) return;

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

    if (!frame.visible) return;

    if (mode == GizmoMode::Rotate)
    {
        appendRotationRings(frame, style, *camera, vertices);
        return;
    }

    if (mode != GizmoMode::Move) return;
    
    const auto &arrow = moveArrowMesh();
    vertices.reserve(arrow.size() * 3 + 120);

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

    struct Plane
    {
        glm::vec3 origin;
        glm::vec3 u;
        glm::vec3 v;
        glm::vec3 color;
        float depth;
    };

    std::array<Plane, 3> planes{};
    const float length = frame.axisLengthWorld;
    const float size = length * style.planeSize;

    if (!std::isfinite(size) || size <= 0.0f) return;

    const float borderInset = std::clamp(style.borderWidthPixels / (style.planeSize * style.axisLengthPixels), 0.0f, 0.49f);

    for (int normal = 0; normal < 3; ++normal)
    {
        const glm::vec3 u = frame.basis[(normal + 1) % 3];
        const glm::vec3 v = frame.basis[(normal + 2) % 3];

        const glm::vec3 origin = frame.pivot + (u + v) * length * style.planeOffset;
        const glm::vec3 centre = origin + (u + v) * size * 0.5f;

        planes[normal] = {
            origin, u * size, v * size,
            style.axisColors[normal],
            glm::dot(centre - camera->getPosition(), camera->getFront())};

        appendBorder(vertices, origin, u * size, v * size, borderInset, glm::vec4(style.axisColors[normal], 1.0f));
    }

    const glm::mat4 inverseView = glm::inverse(camera->getViewMatrix());

    const float centreSize = length * style.centreSizePixels / style.axisLengthPixels;

    if (std::isfinite(centreSize) && centreSize > 0.0f)
    {
        const glm::vec3 u = glm::vec3(inverseView[0]) * centreSize;
        const glm::vec3 v = glm::vec3(inverseView[1]) * centreSize;
        const glm::vec3 origin = frame.pivot - (u + v) * 0.5f;

        const float inset = std::clamp(style.borderWidthPixels / style.centreSizePixels, 0.0f, 0.49f);

        appendBorder(vertices, origin, u, v, inset, glm::vec4{0.90f, 0.92f, 0.95f, 1.0f});
        appendQuad(vertices, origin + (u + v) * inset, u * (1.0f - 2.0f * inset), v * (1.0f - 2.0f * inset), glm::vec4{0.12f, 0.14f, 0.17f, 1.0f});
    }

    std::sort(planes.begin(), planes.end(),
              [](const Plane &a, const Plane &b)
              {
                  return a.depth > b.depth;
              });

    for (const auto &plane : planes)
    {
        appendQuad(vertices, plane.origin, plane.u, plane.v, glm::vec4(plane.color, style.planeOpacity));
    }

}
} // namespace Cthulhu::Editor
