#pragma once

#include <span>
#include <vector>
#include <cstddef>
#include <cstdint>

#include <glm.hpp>

#include "entityId.hpp"
#include "overlayVertex.hpp"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Editor
{
struct PickingRay;

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

enum class GizmoHandle : std::uint8_t
{
    None,
    X,
    Y,
    Z,
    XY,
    YZ,
    ZX,
    Centre
};

struct GizmoPart
{
    GizmoHandle handle = GizmoHandle::None;
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;
};

struct GizmoStyle
{
    float axisLengthPixels = 96.0f;
    float hitPaddingPixels = 5.0f;
    float hoverBiasPixels = 1.5f;
    float planeOffset = 0.20f;
    float planeSize = 0.18f;
    float planeOpacity = 0.18f;
    float centreSizePixels = 10.0f;
    float borderWidthPixels = 1.25f;

    float rotationRadius = 0.80f;
    float rotationThicknessPixels = 2.5f;
    float rotationRearBrightness = 0.40f;
    
    float scaleShaftWidthPixels = 2.5f;
    float scaleHandleSizePixels = 9.0f;
    float scaleCentreSizePixels = 11.0f;

    glm::vec3 hoverColor{1.0f, 0.78f, 0.15f};
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
    std::vector<GizmoPart> parts;
    GizmoHandle hovered = GizmoHandle::None;

    void update(Engine &engine, std::span<const Scene::EntityId> selection, GizmoMode mode, GizmoSpace space, float viewportHeight);
    
    [[nodiscard]] GizmoHandle pick(const PickingRay &ray, float maxDistance, const glm::mat4 &viewProjection, const glm::vec2 &viewportSize, 
                                    const glm::vec2 mousePosition, GizmoHandle previous) const;
};
} // namespace Cthulhu::Editor
