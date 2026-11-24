#pragma once

#include <string_view>

#include "OctoGui/Context.h"

namespace Cthulhu::Editor
{
struct IconButton
{
    octogui::NodeHandle button{};
    octogui::NodeHandle icon{};
};

octogui::NodeHandle createTitledPanel(
    octogui::Context &ctx,
    octogui::NodeHandle pane,
    std::string_view icon,
    std::string_view title,
    IconButton *action = nullptr);

void styleGhostButton(octogui::Context &ctx, octogui::NodeHandle button);

enum class ButtonRole
{
    Neutral,     // standard raised button
    Primary,     // the main action: accent fill
    Destructive, // removes or discards something: red tint
    Ghost,       // cancel / dismiss: no fill until hovered
};

void styleButtonRole(octogui::Context &ctx, octogui::NodeHandle button, ButtonRole role);

void styleSplitter(octogui::Context &ctx, octogui::NodeHandle split);
} // namespace Cthulhu::Editor
