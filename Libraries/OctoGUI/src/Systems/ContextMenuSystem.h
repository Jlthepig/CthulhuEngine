#pragma once

#include <limits>
#include <span>
#include <string>
#include <vector>

#include "OctoGui/ContextMenu.h"
#include "OctoGui/Rect.h"

namespace octogui {

class DrawList;
class Font;
class Tree;
struct InputState;
struct Theme;

class ContextMenuSystem {
public:
    bool open(const Tree& tree, NodeHandle root, NodeHandle owner, Rect viewport, Vec2 position, std::span<const ContextMenuItem> source, Font& font);

    void close() noexcept;

    bool ownsInput() const noexcept {
        return opened || captured || draining;
    }

    void update(const Tree& tree, NodeHandle currentRoot, Rect viewport, const InputState& input, Font* font);

    bool poll(const Tree& tree, NodeHandle currentRoot, ContextMenuSelection& out) noexcept;

    void paint(const Tree& tree, NodeHandle currentRoot, Font* font, const Theme& theme, DrawList& list) const;

private:
    struct Item {
        u64 action{};
        std::string label;
        bool enabled = true;
    };

    static constexpr usize NoItem = std::numeric_limits<usize>::max();

    bool reflow(Rect viewport);
    Rect rowRect(usize index) const noexcept;
    usize itemAt(Vec2 point) const noexcept;
    void moveSelection(int direction);
    void revealSelection();
    void activate(usize index);

    std::vector<Item> items;

    NodeHandle root{};
    NodeHandle owner{};
    Vec2 requestedPosition{};
    Rect viewportRect{};
    Rect box{};

    f32 preferredWidth = 160.0f;
    f32 fullHeight = 0.0f;
    f32 rowHeight = 28.0f;
    f32 scrollY{};
    f32 maxScroll{};

    usize active = NoItem;
    usize pressed = NoItem;

    bool opened = false;
    bool captured = false;
    bool draining = false;
    bool pending = false;
    ContextMenuSelection selection{};
};

} // namespace octogui