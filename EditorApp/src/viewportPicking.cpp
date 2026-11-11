#include "viewportPanel.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include <gtc/matrix_transform.hpp>

#include "components.hpp"
#include "engine.hpp"
#include "hierarchyPanel.hpp"
#include "model.hpp"
#include "pickingMath.hpp"
#include "scene.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
namespace
{
bool boundsOverlapMarquee(const glm::mat4 &mvp,
                          const glm::vec3 &minimum,
                          const glm::vec3 &maximum,
                          const octogui::Rect &box,
                          const octogui::Rect &viewport)
{
    const double left = 2.0 * (box.x - viewport.x) / viewport.w - 1.0;
    const double right = 2.0 * (box.x + box.w - viewport.x) / viewport.w - 1.0;
    const double top = 1.0 - 2.0 * (box.y - viewport.y) / viewport.h;
    const double bottom = 1.0 - 2.0 * (box.y + box.h - viewport.y) / viewport.h;

    const glm::dmat4 rows = glm::transpose(glm::dmat4(mvp));
    const glm::dvec4 planes[] = {rows[0] - left * rows[3],
                                 right * rows[3] - rows[0],
                                 rows[1] - bottom * rows[3],
                                 top * rows[3] - rows[1],
                                 rows[3] + rows[2],
                                 rows[3] - rows[2]};

    for (const auto &plane : planes)
    {
        const glm::dvec3 support{plane.x >= 0.0 ? maximum.x : minimum.x,
                                 plane.y >= 0.0 ? maximum.y : minimum.y,
                                 plane.z >= 0.0 ? maximum.z : minimum.z};
        const double distance = glm::dot(glm::dvec3(plane), support) + plane.w;
        if (!std::isfinite(distance) || distance < 0.0)
            return false;
    }
    return true;
}

struct ViewportProjection
{
    glm::mat4 matrix;
    octogui::Rect rect;
};

std::optional<ViewportProjection> viewportProjection(const octogui::Context &ctx,
                                                      Engine &engine,
                                                      const octogui::Rect &rect)
{
    const auto *camera = engine.getCamera();
    const auto *window = engine.getWindow();
    GLFWwindow *native = window ? window->getWindow() : nullptr;
    const auto display = ctx.input().displaySize;
    if (!camera || !native || glfwGetInputMode(native, GLFW_CURSOR) != GLFW_CURSOR_NORMAL ||
        rect.w <= 0.0f || rect.h <= 0.0f || display.x <= 0.0f || display.y <= 0.0f)
    {
        return std::nullopt;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(native, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0)
        return std::nullopt;

    const float sx = static_cast<float>(framebufferWidth) / display.x;
    const float sy = static_cast<float>(framebufferHeight) / display.y;
    const int x = std::clamp(static_cast<int>(rect.x * sx), 0, framebufferWidth - 1);
    const int y = std::clamp(static_cast<int>(rect.y * sy), 0, framebufferHeight - 1);
    const int width = std::min(static_cast<int>(rect.w * sx), framebufferWidth - x);
    const int height = std::min(static_cast<int>(rect.h * sy), framebufferHeight - y);
    if (width <= 0 || height <= 0)
        return std::nullopt;

    const auto &config = engine.getRenderer().getConfig();
    const auto projection = glm::perspective(glm::radians(camera->getFov()),
                                             static_cast<float>(width) / static_cast<float>(height),
                                             config.nearPlane, config.farPlane);
    return ViewportProjection{projection * camera->getViewMatrix(),
                              {static_cast<float>(x) / sx, static_cast<float>(y) / sy,
                               static_cast<float>(width) / sx, static_cast<float>(height) / sy}};
}

struct ViewportRay
{
    PickingRay ray;
    float length;
};

std::optional<ViewportRay> makeViewportRay(const ViewportProjection &viewport, octogui::Vec2 pointer)
{
    const auto &rect = viewport.rect;
    const float x = 2.0f * (pointer.x - rect.x) / rect.w - 1.0f;
    const float y = 1.0f - 2.0f * (pointer.y - rect.y) / rect.h;
    const auto inverse = glm::inverse(viewport.matrix);
    const auto nearPoint = inverse * glm::vec4(x, y, -1.0f, 1.0f);
    const auto farPoint = inverse * glm::vec4(x, y, 1.0f, 1.0f);
    if (nearPoint.w == 0.0f || farPoint.w == 0.0f)
        return std::nullopt;

    const glm::vec3 origin(nearPoint / nearPoint.w);
    const glm::vec3 segment = glm::vec3(farPoint / farPoint.w) - origin;
    const float length = glm::length(segment);
    if (!std::isfinite(length) || length <= 0.0f)
        return std::nullopt;

    return ViewportRay{{origin, segment / length}, length};
}

struct SceneHit
{
    Scene::EntityId entity;
    float distance;
    glm::vec3 normal;
};

std::optional<SceneHit> pickMesh(const Scene::Scene &scene, const Assets::AssetManager &assets, const ViewportRay &sample, bool frontFacesOnly, Scene::EntityId only = {})
{
    float nearest = sample.length;
    std::optional<SceneHit> hit;

    const auto testMesh = [&](flecs::entity entity, const Scene::TransformComponent &transform, const Scene::MeshComponent &, 
                                              const Scene::MeshRuntimeComponent &runtime) {
        if (!entity.has<Scene::TagActive>())
            return;

        const auto *identity = entity.try_get<Scene::EntityIdentityComponent>();
        const auto *model = assets.getModel(runtime.model);
        if (!identity || !model)
            return;

        const float determinant = glm::determinant(transform.cachedModelMatrix);
        if (!std::isfinite(determinant) || determinant == 0.0f)
            return;

        const glm::mat4 inverse = glm::inverse(transform.cachedModelMatrix);
        // keep local direction unnormalized distance stays in world units
        const PickingRay localRay{glm::vec3(inverse * glm::vec4(sample.ray.origin, 1.0f)),
                                 glm::vec3(inverse * glm::vec4(sample.ray.direction, 0.0f))};
        if (!rayIntersectsBounds(localRay, model->boundsMin, model->boundsMax, nearest))
            return;

        for (const auto &mesh : model->meshes)
        {
            const auto indices = mesh.getIndices();
            const std::size_t end = indices.size() - indices.size() % 3;
            for (std::size_t i = 0; i < end; i += 3)
            {
                glm::vec3 a, b, c;
                float distance = 0.0f;
                if (!mesh.getPosition(indices[i], a) || !mesh.getPosition(indices[i + 1], b) ||
                    !mesh.getPosition(indices[i + 2], c) ||
                    !rayIntersectsTriangle(localRay, a, b, c, nearest, distance))
                {
                    continue;
                }

                glm::vec3 normal{0.0f};
                if (frontFacesOnly)
                {
                    const auto localNormal = glm::cross(glm::dvec3(b) - glm::dvec3(a),
                                                        glm::dvec3(c) - glm::dvec3(a));
                    if (glm::dot(localNormal, glm::dvec3(localRay.direction)) >= 0.0)
                        continue;

                    const auto worldNormal = glm::transpose(glm::dmat3(inverse)) * localNormal;
                    const double length = glm::length(worldNormal);
                    if (!std::isfinite(length) || length <= 0.0)
                        continue;

                    normal = glm::vec3(worldNormal / length);
                }

                nearest = distance;
                hit = SceneHit{identity->id, distance, normal};
            }
        }
    };

    if (only.isValid())
    {
        const auto entity = scene.findEntity(only);
        const auto *transform = entity ? entity->try_get<Scene::TransformComponent>() : nullptr;
        const auto *mesh = entity ? entity->try_get<Scene::MeshComponent>() : nullptr;
        const auto *runtime = entity ? entity->try_get<Scene::MeshRuntimeComponent>() : nullptr;
        if (transform && mesh && runtime)
            testMesh(*entity, *transform, *mesh, *runtime);
    }
    else
    {
        scene.getWorld().each(testMesh);
    }

    return hit;
}
} // namespace

void ViewportPanel::updatePicking(octogui::Context &ctx,
                                  Engine &engine,
                                  HierarchyPanel &hierarchy,
                                  octogui::NodeHandle sceneNode)
{
    if (ctx.contextMenuOwnsInput() ||engine.isPlaying() || (!completedClick && !completedMarquee))
    {
        return;
    }

    const auto *scene = engine.getActiveScene();
    const auto click = completedClick.value_or(marqueeStart);
    const auto viewport = viewportProjection(ctx, engine, ctx.tree().rect(sceneNode));
    if (!scene || !viewport || !viewport->rect.contains(click))
        return;

    if (completedMarquee)
    {
        std::vector<Scene::EntityId> entities;
        if (selectionAdditive)
            entities = hierarchy.selection();

        scene->getWorld().each([&](flecs::entity entity,
                                   const Scene::TransformComponent &transform,
                                   const Scene::MeshComponent &,
                                   const Scene::MeshRuntimeComponent &runtime) {
            if (!entity.has<Scene::TagActive>())
                return;
            const auto *identity = entity.try_get<Scene::EntityIdentityComponent>();
            const auto *model = engine.getAssetManager().getModel(runtime.model);
            if (identity && model &&
                boundsOverlapMarquee(viewport->matrix * transform.cachedModelMatrix,
                                     model->boundsMin,
                                     model->boundsMax,
                                     *completedMarquee,
                                     viewport->rect))
            {
                entities.push_back(identity->id);
            }
        });
        hierarchy.selectEntities(ctx, std::move(entities));
        return;
    }

    const auto ray = makeViewportRay(*viewport, click);
    if (!ray)
        return;

    const auto hit = pickMesh(*scene, engine.getAssetManager(), *ray, false);
    const auto selected = hit ? std::optional{hit->entity} : std::nullopt;

    if (!selectionAdditive)
    {
        hierarchy.selectEntity(ctx, selected);
    }
    else if (selected)
    {
        auto entities = hierarchy.selection();
        const auto found = std::find(entities.begin(), entities.end(), *selected);

        if (found == entities.end())
            entities.push_back(*selected);
        else if (selectionCtrl)
            entities.erase(found);

        hierarchy.selectEntities(ctx, std::move(entities));
    }
}

bool ViewportPanel::isAssetDropTarget(const octogui::Context &ctx, octogui::NodeHandle sceneNode) const
{
    const auto pointer = ctx.input().mouse.position;
    const auto hovered = ctx.hovered();
    return ctx.visibleRect(sceneNode).contains(pointer) && !ctx.visibleRect(header).contains(pointer) &&
                          (!ctx.isValid(hovered) || hovered == sceneNode || hovered == overlayNode);
}

std::optional<glm::vec3> ViewportPanel::modelDropPosition(const octogui::Context &ctx, Engine &engine, octogui::NodeHandle sceneNode, const Rendering::Model &model) const
{
    const auto *scene = engine.getActiveScene();
    const auto viewport = viewportProjection(ctx, engine, ctx.tree().rect(sceneNode));
    const auto pointer = ctx.input().mouse.position;
    if (!scene || !viewport || !viewport->rect.contains(pointer))
        return std::nullopt;

    const auto sample = makeViewportRay(*viewport, pointer);
    if (!sample)
        return std::nullopt;

    const auto &ray = sample->ray;
    const float nearOffset = glm::dot(ray.origin - engine.getCamera()->getPosition(), ray.direction);
    const auto &assets = engine.getAssetManager();
    const auto hit = pickMesh(*scene, assets, *sample, true);
    const bool snap = ctx.input().modifiers.ctrl && std::isfinite(snapMoveValue) && snapMoveValue > 0.0f;

    glm::vec3 anchor;
    glm::vec3 normal{0.0f};
    if (hit)
    {
        anchor = ray.origin + ray.direction * hit->distance;
        normal = hit->normal;
    }
    else
    {
        constexpr float gridReach = 50.0f;
        constexpr float floatingDistance = 5.0f;
        const float gridDistance = std::abs(ray.direction.y) > 0.01f ? -ray.origin.y / ray.direction.y : -1.0f;
        if (gridDistance >= 0.0f && gridDistance <= std::min(sample->length, gridReach - nearOffset))
        {
            anchor = ray.origin + ray.direction * gridDistance;
            anchor.y = 0.0f;
            normal.y = 1.0f;
        }
        else
        {
            anchor = ray.origin + ray.direction * std::clamp(floatingDistance - nearOffset, 0.0f, sample->length);
        }
    }

    if (snap)
    {
        const glm::vec3 snapped = glm::round(anchor / snapMoveValue) * snapMoveValue;
        if (hit)
        {
            const glm::vec3 onPlane = snapped - normal * glm::dot(snapped - anchor, normal);
            // keep original anchor if snapping would leave the supporting surface
            constexpr float probeOffset = 0.001f;
            const ViewportRay probe{{onPlane + normal * probeOffset, -normal}, 2.0f * probeOffset};
            const auto contact = pickMesh(*scene, assets, probe, true, hit->entity);
            if (contact && glm::dot(normal, contact->normal) > 0.999f)
                anchor = probe.ray.origin + probe.ray.direction * contact->distance;
        }
        else
        {
            anchor = snapped;
        }
    }

    if (normal != glm::vec3(0.0f))
    {
        float support = std::numeric_limits<float>::infinity();
        for (const auto &mesh : model.meshes)
        {
            const auto indices = mesh.getIndices();
            const std::size_t end = indices.size() - indices.size() % 3;
            for (const auto index : indices.first(end))
            {
                glm::vec3 vertex;
                if (mesh.getPosition(index, vertex))
                    support = std::min(support, glm::dot(normal, vertex));
            }
        }

        if (!std::isfinite(support))
            return std::nullopt;

        anchor -= normal * support;
    }

    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y) || !std::isfinite(anchor.z))
        return std::nullopt;

    return anchor;
}

} // namespace Cthulhu::Editor
