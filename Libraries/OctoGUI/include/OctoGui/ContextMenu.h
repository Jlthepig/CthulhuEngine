#pragma once

#include <string_view>

#include "OctoGui/Node.h"

namespace octogui {

struct ContextMenuItem {
    u64 action{};
    std::string_view label;
    bool enabled = true;
};

struct ContextMenuSelection {
    NodeHandle owner{};
    u64 action{};
};

} // namespace octogui