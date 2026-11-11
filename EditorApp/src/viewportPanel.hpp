#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "gizmoSystem.hpp"
#include "transformValue.hpp"

#include "OctoGui/Context.h"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Rendering
{
struct Model;
}
namespace Cthulhu::Scribe
{
class Session;
}

namespace Cthulhu::Editor
{

class HierarchyPanel;

class ViewportPanel
{
  public:
    void build(octogui::Context &ctx, octogui::NodeHandle viewportRoot);
    void update(octogui::Context &ctx);

    [[nodiscard]] bool gizmoDragActive() const noexcept
    {
        return drag.handle != GizmoHandle::None;
    }

    bool finishGizmoDrag(Scribe::Session &session, bool commit);
    void drawGizmoFeedback(octogui::Context &ctx) const;

    [[nodiscard]] std::span<const Rendering::OverlayVertex> overlayVertices() const noexcept
    {
        return gizmoSystem.vertices;
    }

    void updatePicking(octogui::Context &ctx, Engine &engine, HierarchyPanel &hierarchy, octogui::NodeHandle sceneNode);
    [[nodiscard]] bool isAssetDropTarget(const octogui::Context &ctx, octogui::NodeHandle sceneNode) const;
    [[nodiscard]] std::optional<glm::vec3> modelDropPosition(const octogui::Context &ctx, Engine &engine, octogui::NodeHandle sceneNode,const Rendering::Model &model) const;

    void updateMarquee(octogui::Context &ctx, octogui::NodeHandle sceneNode, bool enabled, std::uint64_t sceneGeneration);
    void updateGizmo(octogui::Context &ctx, Engine &engine, Scribe::Session &session, const HierarchyPanel &hierarchy, octogui::NodeHandle sceneNode);

  private:
    octogui::NodeHandle overlayNode{};
    octogui::NodeHandle header{};

    octogui::NodeHandle selectButton{};
    octogui::NodeHandle moveButton{};
    octogui::NodeHandle rotateButton{};
    octogui::NodeHandle scaleButton{};
    octogui::NodeHandle selectIcon{};
    octogui::NodeHandle moveIcon{};
    octogui::NodeHandle rotateIcon{};
    octogui::NodeHandle scaleIcon{};

    octogui::NodeHandle worldButton{};
    octogui::NodeHandle localButton{};

    octogui::NodeHandle snapMoveInput{};
    octogui::NodeHandle snapRotateInput{};
    octogui::NodeHandle snapScaleInput{};

    GizmoSystem gizmoSystem;

    GizmoMode currentMode = GizmoMode::Select;
    GizmoSpace currentSpace = GizmoSpace::World;
    float snapMoveValue = 0.5f;
    float snapRotateValue = 15.0f;
    float snapScaleValue = 0.1f;
    bool snapActive = false;
    struct LinearDrag
    {
        glm::mat4 inverseViewProjection{1.0f};
        glm::dvec3 u{}, v{}, normal{};
        glm::dvec2 start{};
        double unitsPerPixel = 0.0;
        bool vertical = false;
    };
    struct RotateDrag
    {
        glm::mat4 inverseViewProjection{1.0f};
        glm::dvec3 axis{}, u{}, v{};
        double previousAngle = 0.0;
        double startAngle = 0.0;
        double appliedAngle = 0.0;
        double angle = 0.0;
        double radiansPerPixel = 0.0;
        bool vertical = false;
        bool tracking = false;
        std::vector<glm::dvec3> offsets;
        std::vector<glm::dmat3> initialRotations;
        std::vector<glm::dmat3> inverseFrames;
    };
    struct ScaleDrag
    {
        struct Root
        {
            glm::dmat3 worldLinear{1.0};
            glm::dvec3 offset{};
        };

        bool uniform = false;
        glm::dvec3 axis{};
        double minimumFactor = 0.0001;
        double maximumFactor = 1.0;
        std::vector<Root> roots;
    };
    struct GizmoDrag
    {
        GizmoHandle handle = GizmoHandle::None;
        GizmoMode mode = GizmoMode::Select;
        GizmoSpace space = GizmoSpace::World;
        GizmoFrame frame;
        glm::mat4 viewProjection{1.0f};
        glm::dvec3 translation{};
        octogui::Rect viewport{};
        LinearDrag linear;
        RotateDrag rotate;
        ScaleDrag scale;
        glm::mat3 cameraBasis{1.0f};
        octogui::Vec2 mouseStart{};
        std::uint64_t generation = 0;
        std::vector<Scene::EntityId> selection;
        std::vector<Scribe::TransformValue> values;
        std::vector<glm::dmat3> inverseParents;
        std::vector<std::uint8_t> followsSelection;
    };

    GizmoDrag drag;

    bool gizmoCaptured = false;

    bool marqueePending = false;
    bool marqueeActive = false;
    octogui::Vec2 marqueeStart{};
    std::optional<octogui::Rect> completedMarquee;
    std::optional<octogui::Vec2> completedClick;
    bool selectionCtrl = false;
    bool selectionAdditive = false;
    std::uint64_t marqueeGeneration = 0;

    [[nodiscard]] std::optional<glm::dvec2> linearCoordinates(octogui::Vec2 point) const;
    bool beginLinearDrag(Engine &engine, const glm::mat4 &inverseViewProjection, const octogui::Rect &viewport);
    bool updateMoveDrag(octogui::Context &ctx, Scribe::Session &session);
    bool prepareDragParents(Engine &engine);
    [[nodiscard]] std::optional<double> rotateAngle(octogui::Vec2 point) const;
    bool beginRotateDrag(Engine &engine, const glm::mat4 &inverseViewProjection, const octogui::Rect &viewport);
    bool updateRotateDrag(octogui::Context &ctx, Scribe::Session &session);
    bool beginScaleDrag(Engine &engine, const glm::mat4 &inverseViewProjection, const octogui::Rect &viewport);
    bool updateScaleDrag(octogui::Context &ctx, Scribe::Session &session);

    void setMode(octogui::Context &ctx, GizmoMode mode);
    void setSpace(octogui::Context &ctx, GizmoSpace space);
    void refreshToggles(octogui::Context &ctx);
};
} // namespace Cthulhu::Editor
