#pragma once

#include <span>
#include <vector>

#include <glm.hpp>

#include "entityId.hpp"
#include "overlayVertex.hpp"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Editor
{
enum class GizmoMode
{
    Select = 0,
    Move,
    Rotate,
    Scale
};

enum class GizmoSpace
{
    World = 0,
    Local
};

struct GizmoStyle
{
    float axisLengthPixels = 96.0f;
    glm::vec3 axisColors[3]{{0.92f, 0.30f, 0.28f}, {0.40f, 0.82f, 0.35f}, {0.30f, 0.56f, 0.96f}};
};

struct GizmoFrame
{
    glm::vec3 pivot{};
    glm::mat3 basis{1.0f};
    float axisLengthWorld = 0.0f;
    bool visible = false;
};

struct GizmoSystem
{
    GizmoStyle style;
    GizmoFrame frame;
    std::vector<Rendering::OverlayVertex> vertices;

    void update(Engine &engine,
                std::span<const Scene::EntityId> selection,
                GizmoMode mode,
                GizmoSpace space,
                float viewportHeight);
};
} // namespace Cthulhu::Editor
