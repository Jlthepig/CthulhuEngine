#pragma once

#include <string>

#include "OctoGui/Context.h"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scribe
{
class Session;
}

namespace Cthulhu::Editor
{
// the editor's top bar, resizable workspace panes, status bar
class EditorShell
{
  public:
    void build(octogui::Context &ctx);

    void update(octogui::Context &ctx, Engine &engine, Scribe::Session &session);

    [[nodiscard]] octogui::NodeHandle hierarchyPanel() const noexcept
    {
        return hierarchy;
    }
    [[nodiscard]] octogui::NodeHandle sceneArea() const noexcept
    {
        return sceneNode;
    }
    [[nodiscard]] octogui::NodeHandle inspectorPanel() const noexcept
    {
        return inspector;
    }

  private:
    octogui::NodeHandle saveButton{};
    octogui::NodeHandle playButton{};
    octogui::NodeHandle statusLabel{};

    octogui::NodeHandle hierarchy{};
    octogui::NodeHandle sceneNode{};
    octogui::NodeHandle inspector{};

    std::string lastStatus;
    bool lastPlaying = false;
};
} // namespace Cthulhu::Editor