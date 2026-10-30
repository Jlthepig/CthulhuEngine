#include "viewportPanel.hpp"

#include <cmath>
#include <optional>
#include <algorithm>
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
     const glm::vec3 &minimum, const glm::vec3 &maximum, const octogui::Rect &box, const octogui::Rect &viewport)
{
    const double left = 2.0 * (box.x - viewport.x) / viewport.w - 1.0;
    const double right = 2.0 * (box.x + box.w - viewport.x) / viewport.w - 1.0;
    const double top = 1.0 - 2.0 * (box.y - viewport.y) / viewport.h;
    const double bottom = 1.0 - 2.0 * (box.y + box.h - viewport.y) / viewport.h;

    const glm::dmat4 rows = glm::transpose(glm::dmat4(mvp));
    const glm::dvec4 planes[] = {
        rows[0] - left * rows[3], right * rows[3] - rows[0],
        rows[1] - bottom * rows[3], top * rows[3] - rows[1],
        rows[3] + rows[2], rows[3] - rows[2]};

    for (const auto &plane : planes)
    {
        const glm::dvec3 support{
            plane.x >= 0.0 ? maximum.x : minimum.x,
            plane.y >= 0.0 ? maximum.y : minimum.y,
            plane.z >= 0.0 ? maximum.z : minimum.z};
        const double distance = glm::dot(glm::dvec3(plane), support) + plane.w;
        if (!std::isfinite(distance) || distance < 0.0)
            return false;
    }
    return true;
}
}

void ViewportPanel::updatePicking(octogui::Context &ctx, Engine &engine, HierarchyPanel &hierarchy, octogui::NodeHandle sceneNode)
{
    if (engine.isPlaying() || (!completedClick && !completedMarquee))
    {
        return;
    }

    auto *scene = engine.getActiveScene();
    auto *camera = engine.getCamera();
    auto *window = engine.getWindow();

    if (!scene || !camera || !window) return;

    GLFWwindow *nativeWindow = window->getWindow();
    if (glfwGetInputMode(nativeWindow, GLFW_CURSOR) != GLFW_CURSOR_NORMAL) return;

    const auto rect = ctx.tree().rect(sceneNode);
    const auto display = ctx.input().displaySize;

    if (rect.w <= 0.0f || rect.h <= 0.0f || display.x <= 0.0f || display.y <= 0.0f)
    {
        return;
    }

    const auto click = completedClick.value_or(marqueeStart);
    const float x = (click.x - rect.x) / rect.w;
    const float y = (click.y - rect.y) / rect.h;

    if (x < 0.0f || x >= 1.0f || y < 0.0f || y >= 1.0f) return;

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(nativeWindow, &framebufferWidth, &framebufferHeight);

    const int width = static_cast<int>(rect.w * static_cast<float>(framebufferWidth) / display.x);
    const int height = static_cast<int>(rect.h * static_cast<float>(framebufferHeight) / display.y);

    if (width <= 0 || height <= 0) return;

    const auto &config = engine.getRenderer().getConfig();
    const glm::mat4 projection = glm::perspective( camera->getFov(), static_cast<float>(width) / static_cast<float>(height), 
    config.nearPlane, config.farPlane);

    const glm::mat4 viewProjection = projection * camera->getViewMatrix();

    if (completedMarquee)
    {
        std::vector<Scene::EntityId> entities;
        if (selectionAdditive)
            entities = hierarchy.selection();

        scene->getWorld().each(
            [&](flecs::entity entity, const Scene::TransformComponent &transform,
                const Scene::MeshComponent &, const Scene::MeshRuntimeComponent &runtime)
            {
                if (!entity.has<Scene::TagActive>())
                    return;
                const auto *identity = entity.try_get<Scene::EntityIdentityComponent>();
                const auto *model = engine.getAssetManager().getModel(runtime.model);
                if (identity && model && boundsOverlapMarquee(
                        viewProjection * transform.cachedModelMatrix,
                        model->boundsMin, model->boundsMax, *completedMarquee, rect))
                {
                    entities.push_back(identity->id);
                }
            });
        hierarchy.selectEntities(ctx, std::move(entities));
        return;
    }

    const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

    const float ndcX = x * 2.0f - 1.0f;
    const float ndcY = 1.0f - y * 2.0f;

    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

    if (nearPoint.w == 0.0f || farPoint.w == 0.0f) return;

    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::vec3 segment = glm::vec3(farPoint - nearPoint);
    float nearestDistance = glm::length(segment);

    if (!std::isfinite(nearestDistance) || nearestDistance <= 0.0f) return;

    // start at the near plane and stop at the far plane
    const PickingRay worldRay{glm::vec3(nearPoint), segment / nearestDistance};

    std::optional<Scene::EntityId> selected;

    scene->getWorld().each(
        [&](flecs::entity entity, const Scene::TransformComponent &transform, const Scene::MeshComponent &, const Scene::MeshRuntimeComponent &runtime)
        {
            if (!entity.has<Scene::TagActive>()) return;

            const auto *identity = entity.try_get<Scene::EntityIdentityComponent>();
            const auto *model = engine.getAssetManager().getModel(runtime.model);

            if (!identity || !model) return;

            const float determinant = glm::determinant(transform.cachedModelMatrix);

            // zero scale transforms cant be inverted
            if (!std::isfinite(determinant) || determinant == 0.0f) return;

            const glm::mat4 inverseModel = glm::inverse(transform.cachedModelMatrix);

            const PickingRay localRay{
                glm::vec3(inverseModel * glm::vec4(worldRay.origin, 1.0f)),glm::vec3(inverseModel * glm::vec4(worldRay.direction, 0.0f))};

            // Keep the local direction unnormalized so hit distances
            // remains same between differently sized entities
            if (!rayIntersectsBounds( localRay, model->boundsMin, model->boundsMax, nearestDistance))
            {
                return;
            }

            for (const auto &mesh : model->meshes)
            {
                const auto indices = mesh.getIndices();
                const size_t triangleEnd = indices.size() - indices.size() % 3;

                for (size_t i = 0; i < triangleEnd; i += 3)
                {
                    glm::vec3 a, b, c;
                    if (!mesh.getPosition(indices[i], a) || !mesh.getPosition(indices[i + 1], b) ||
                        !mesh.getPosition(indices[i + 2], c))
                    {
                        continue;
                    }

                    float distance = 0.0f;
                    if (rayIntersectsTriangle(localRay, a, b, c, nearestDistance, distance))
                    {
                        nearestDistance = distance;
                        selected = identity->id;
                    }
                }
            }
        });

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
}
