#include "viewportPanel.hpp"

#include <cmath>
#include <optional>

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
void ViewportPanel::updatePicking(octogui::Context &ctx, Engine &engine, HierarchyPanel &hierarchy, octogui::NodeHandle sceneNode)
{
    using octogui::MouseButton;

    const auto &mouse = ctx.input().mouse;

    if (engine.isPlaying() || !mouse.pressed(MouseButton::Left) || mouse.isDown(MouseButton::Right))
    {
        return;
    }

    const auto hovered = ctx.hovered();
    if (ctx.isValid(hovered) && hovered != sceneNode && hovered != overlayNode) return;

    if (ctx.tree().rect(header).contains((mouse.position))) return ;

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

    const float x = (mouse.position.x - rect.x) / rect.w;
    const float y = (mouse.position.y - rect.y) / rect.h;

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

    const glm::mat4 inverseViewProjection = glm::inverse(projection * camera->getViewMatrix());

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

    hierarchy.selectEntity(ctx, selected);
}
}