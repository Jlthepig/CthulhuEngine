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
    IconButton *leadingAction = nullptr);
} // namespace Cthulhu::Editor
