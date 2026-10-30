#pragma once

#include <cstdint>
#include <optional>

#include "OctoGui/Context.h"

namespace Cthulhu { class Engine; }
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

class HierarchyPanel;
class ViewportPanel
{
  public:
    void build(octogui::Context &ctx, octogui::NodeHandle viewportRoot);
    void update(octogui::Context &ctx);

    [[nodiscard]] GizmoMode mode() const noexcept { return currentMode; }
    [[nodiscard]] GizmoSpace space() const noexcept { return currentSpace; }
    [[nodiscard]] float snapMove() const noexcept { return snapMoveValue; }
    [[nodiscard]] float snapRotate() const noexcept { return snapRotateValue; }
    [[nodiscard]] float snapScale() const noexcept { return snapScaleValue; }
    [[nodiscard]] bool snapEnabled() const noexcept { return snapActive; }

    [[nodiscard]] octogui::NodeHandle overlay() const noexcept { return overlayNode; }

    void updatePicking(octogui::Context &ctx, Engine &engine, HierarchyPanel &hierarchy, octogui::NodeHandle sceneNode);
    void updateMarquee(octogui::Context& ctx, octogui::NodeHandle sceneNode,
                       bool enabled, std::uint64_t sceneGeneration);

  private:
    octogui::NodeHandle overlayNode{};
    octogui::NodeHandle header{};
    octogui::NodeHandle pill{};

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

    GizmoMode currentMode = GizmoMode::Select;
    GizmoSpace currentSpace = GizmoSpace::World;
    float snapMoveValue = 0.5f;
    float snapRotateValue = 15.0f;
    float snapScaleValue = 0.1f;
    bool snapActive = false;

    bool marqueePending = false;
    bool marqueeActive = false;
    octogui::Vec2 marqueeStart{};
    std::optional<octogui::Rect> completedMarquee;
    std::optional<octogui::Vec2> completedClick;
    bool selectionCtrl = false;
    bool selectionAdditive = false;
    std::uint64_t marqueeGeneration = 0;

    void setMode(octogui::Context &ctx, GizmoMode mode);
    void setSpace(octogui::Context &ctx, GizmoSpace space);
    void refreshToggles(octogui::Context &ctx);
};
} // namespace Cthulhu::Editor
