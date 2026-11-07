#include "Systems/TooltipSystem.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Internal/LayoutGeometry.h"
#include "OctoGui/DrawList.h"
#include "OctoGui/Font.h"
#include "OctoGui/Input.h"
#include "OctoGui/Style.h"
#include "OctoGui/Tree.h"
#include "Systems/InteractionSystem.h"

namespace octogui {
namespace {

constexpr f32 TooltipDelay = 0.5f;
constexpr f32 TooltipMovementTolerance = 4.0f;

bool tooltipBlocked(const Tree& tree, const InputState& input, const InteractionState& interaction) {
    if (!input.events.empty() ||
        input.mouse.wheel != Vec2::zero() ||
        tree.isValid(interaction.active) ||
        tree.isValid(interaction.scrollbarActive) ||
        tree.isValid(interaction.splitterActive)) {
        return true;
    }

    for (usize i = 0; i < MouseButtonCount; ++i) {
        if (input.mouse.down[i] || input.mouse.previousDown[i]) {
            return true;
        }
    }

    const ComboBoxState* combo = tree.comboBoxState(interaction.focused);
    return combo && combo->open;
}

} // namespace

const TooltipSystem::Entry*
TooltipSystem::find(NodeHandle node) const noexcept {
    if (node.index >= entryByNode.size()) {
        return nullptr;
    }

    const usize index = entryByNode[node.index];
    if (index >= entries.size() || entries[index].node != node) {
        return nullptr;
    }

    return &entries[index];
}

void TooltipSystem::reset() noexcept {
    target = {};
    anchor = {};
    targetRect = {};
    elapsed = 0.0f;
}

void TooltipSystem::erase(usize index) {
    const NodeHandle removed = entries[index].node;
    entryByNode[removed.index] = NoEntry;

    if (target == removed) {
        reset();
    }

    if (index != entries.size() - 1) {
        entries[index] = std::move(entries.back());
        entryByNode[entries[index].node.index] = index;
    }

    entries.pop_back();
}

void TooltipSystem::set(const Tree& tree, NodeHandle node, std::string_view text) {
    if (!tree.isValid(node)) {
        return;
    }

    if (node.index >= entryByNode.size()) {
        entryByNode.resize(static_cast<usize>(node.index) + 1, NoEntry);
    }

    const usize index = entryByNode[node.index];

    if (text.empty()) {
        if (index < entries.size()) {
            erase(index);
        }
        return;
    }

    if (index < entries.size()) {
        Entry& entry = entries[index];
        if (entry.node == node && entry.text == text) {
            return;
        }

        entry.text.assign(text);
        entry.node = node;
    } else {
        entries.push_back(Entry{node, std::string(text)});
        entryByNode[node.index] = entries.size() - 1;
    }

    if (target.index == node.index) {
        reset();
    }
}

void TooltipSystem::pruneInvalid(const Tree& tree) {
    usize index{};
    while (index < entries.size()) {
        if (!tree.isValid(entries[index].node)) {
            erase(index);
        } else {
            ++index;
        }
    }
}

void TooltipSystem::update(const Tree& tree, const InputState& input, const InteractionState& interaction) {
    if (tooltipBlocked(tree, input, interaction)) {
        reset();
        return;
    }

    NodeHandle next{};
    for (NodeHandle node = interaction.pointerHit;
         tree.isValid(node);
         node = tree.parent(node)) {
        if (find(node)) {
            next = node;
            break;
        }
    }

    if (!tree.isVisibleBranch(next)) {
        reset();
        return;
    }

    const Rect rect = detail::visualRect(tree, next);
    if (!rect.contains(input.mouse.position)) {
        reset();
        return;
    }

    const Vec2 movement = input.mouse.position - anchor;
    const f32 distanceSquared = movement.x * movement.x + movement.y * movement.y;

    if (next != target || rect != targetRect ||
        distanceSquared >
            TooltipMovementTolerance * TooltipMovementTolerance) {
        target = next;
        targetRect = rect;
        anchor = input.mouse.position;
        elapsed = 0.0f;
        return;
    }

    if (std::isfinite(input.deltaTime) && input.deltaTime > 0.0f) {
        elapsed = std::min(TooltipDelay, elapsed + std::min(input.deltaTime, 0.1f));
    }
}

void TooltipSystem::paint(const Tree& tree,
                          NodeHandle root,
                          const InputState& input,
                          const InteractionState& interaction,
                          Font* font,
                          const Theme& theme,
                          DrawList& list) const {
    if (!font || !font->isLoaded() || elapsed < TooltipDelay ||
        !tree.isVisibleBranch(target) ||
        tooltipBlocked(tree, input, interaction)) {
        return;
    }

    const Entry* entry = find(target);
    if (!entry ||
        !detail::visualRect(tree, target).contains(input.mouse.position)) {
        return;
    }

    const Rect viewport = tree.rect(root).intersection(
        Rect{0.0f, 0.0f, input.displaySize.x, input.displaySize.y});
    const Rect bounds = viewport.shrink(4.0f);
    if (bounds.w <= 16.0f || bounds.h <= 12.0f) {
        return;
    }

    const Vec2 textSize = font->measureText(entry->text);
    if (!std::isfinite(textSize.x) || !std::isfinite(textSize.y) ||
        textSize.x < 0.0f || textSize.y <= 0.0f) {
        return;
    }

    const f32 width = std::min(textSize.x + 16.0f, bounds.w);
    const f32 height = std::min(textSize.y + 12.0f, bounds.h);

    f32 y = anchor.y + 18.0f;
    if (y + height > bounds.maxY()) {
        y = anchor.y - height - 12.0f;
    }

    const Rect box{
        std::clamp(anchor.x + 12.0f, bounds.x, bounds.maxX() - width),
        std::clamp(y, bounds.y, bounds.maxY() - height),
        width,
        height};

    list.pushClip(viewport);

    const ShadowStyle& shadow = theme.elevationMedium;
    list.addBoxShadow(box, shadow.color, theme.smallRadius, shadow.offset, shadow.blur, shadow.spread);
    list.addRoundedRectFilled(box, theme.surfaceRaised, theme.smallRadius);
    list.addRoundedRectOutline(box, theme.borderStrong, theme.smallRadius, theme.borderWidth);

    const Rect textRect{ box.x + 8.0f, box.y + 6.0f, box.w - 16.0f, box.h - 12.0f};

    font->drawTextClipped(list, entry->text, textRect.position(), theme.text, textRect);

    list.popClip();
}

} // namespace octogui