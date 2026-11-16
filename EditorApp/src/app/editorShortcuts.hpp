#pragma once

#include <string>
#include "OctoGui/Context.h"

namespace Cthulhu::Editor::Shortcuts
{

enum class Action : octogui::u64
{
    None,
    Undo, Redo,
    NewScene, OpenScene, SaveScene, SaveSceneAs,
    CloseScene, CloseAllScenes, Quit,
    Cut, Copy, Paste, Duplicate, Rename, Delete,
    SelectTool, MoveTool, RotateTool, ScaleTool,
    TogglePlay, ToggleFullscreen, ToggleVSync,
    ResetLayout, ToggleHierarchy, ToggleInspector, ToggleContentBrowser,
    Count
};

[[nodiscard]] Action read(const octogui::InputState &input) noexcept;
[[nodiscard]] Action read(const octogui::Context &ctx) noexcept;
[[nodiscard]] std::string label(Action action);
}