#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "OctoGui/Node.h"
#include "OctoGui/Rect.h"

namespace octogui {

class DrawList;
class Font;
class Tree;
struct InputState;
struct InteractionState;
struct Theme;

class TooltipSystem {
public:
    void set(const Tree& tree, NodeHandle node, std::string_view text);
    void pruneInvalid(const Tree& tree);

    void update(const Tree& tree, const InputState& input, const InteractionState& interaction);
    void paint(const Tree& tree, NodeHandle root, const InputState& input, const InteractionState& interaction, Font* font, const Theme& theme, DrawList& list) const;
    
    void reset() noexcept;
private:
    struct Entry {
        NodeHandle node{};
        std::string text;
    };

    static constexpr usize NoEntry = std::numeric_limits<usize>::max();

    const Entry* find(NodeHandle node) const noexcept;
    void erase(usize index);
    

    std::vector<Entry> entries;
    std::vector<usize> entryByNode;

    NodeHandle target{};
    Vec2 anchor{};
    Rect targetRect{};
    f32 elapsed{};
};

} // namespace octogui