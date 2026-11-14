#pragma once

#include <string_view>
#include <span>

#include "OctoGui/Node.h"

namespace octogui {

struct ContextMenuItem {
    u64 action{};
    std::string_view label;
    bool enabled = true;
    std::span<const ContextMenuItem> children{};
};

struct ContextMenuSelection {
    NodeHandle owner{};
    u64 action{};
};

} // namespace octogui