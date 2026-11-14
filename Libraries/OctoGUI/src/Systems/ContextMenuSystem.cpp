#include "Systems/ContextMenuSystem.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "OctoGui/DrawList.h"
#include "OctoGui/Font.h"
#include "OctoGui/Input.h"
#include "OctoGui/Style.h"
#include "OctoGui/Tree.h"

namespace octogui {
namespace {

bool usableOwner(const Tree& tree, NodeHandle root, NodeHandle owner) {
    for (NodeHandle node = owner;
         tree.isValid(node);
         node = tree.parent(node)) {
        if (!tree.isVisible(node) || !tree.isEnabled(node)) {
            return false;
        }
        if (node == root) {
            return true;
        }
    }
    return false;
}

bool mouseHeld(const InputState& input) {
    for (bool down : input.mouse.down) {
        if (down) {
            return true;
        }
    }
    return false;
}

bool usableViewport(Rect rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
           std::isfinite(rect.w) && std::isfinite(rect.h) &&
           std::isfinite(rect.maxX()) && std::isfinite(rect.maxY()) &&
           rect.w > 32.0f && rect.h > 16.0f;
}

} // namespace

bool ContextMenuSystem::open(const Tree& tree, NodeHandle newRoot, NodeHandle newOwner, Rect viewport, Vec2 position, std::span<const ContextMenuItem> source, Font& font) {
    if (source.empty() || !font.isLoaded() ||
        !usableOwner(tree, newRoot, newOwner) ||
        !usableViewport(viewport) ||
        !std::isfinite(position.x) || !std::isfinite(position.y)) {
        return false;
    }

    const f32 newRowHeight = std::max(28.0f, font.metrics().lineHeight + 8.0f);
    const f32 newHeight = 8.0f + newRowHeight * static_cast<f32>(source.size());

    if (!std::isfinite(newRowHeight) || !std::isfinite(newHeight)) {
        return false;
    }

    std::vector<Item> replacement;
    replacement.reserve(source.size());
    f32 newWidth = 160.0f;

    for (const ContextMenuItem& item : source) {
        if (item.label.empty() ||
            item.label.find_first_of("\r\n") != std::string_view::npos) {
            return false;
        }

        for (const Item& previous : replacement) {
            if (previous.action == item.action) {
                return false;
            }
        }

        const f32 textWidth = font.measureText(item.label).x;
        if (!std::isfinite(textWidth) || textWidth < 0.0f) {
            return false;
        }

        Item copy{item.action, std::string(item.label), item.enabled, {}};
        if (!item.children.empty()) {
            // shortcut >> one submenu level extend when a nested menu is needed.
            for (const auto& child : item.children) {
                if (!child.children.empty()) return false;
            }

            copy.submenu = std::make_unique<ContextMenuSystem>();
            if (!copy.submenu->open(tree, newRoot, newOwner, viewport, position, item.children, font)) {
                return false;
            }
            copy.submenu->close();
        }

        newWidth = std::max(newWidth, textWidth + (copy.submenu ? 44.0f : 24.0f));
        replacement.push_back(std::move(copy));
    }

    hideSubmenu();
    items = std::move(replacement);
    root = newRoot;
    owner = newOwner;
    requestedPosition = position;
    preferredWidth = newWidth;
    rowHeight = newRowHeight;
    fullHeight = newHeight;
    scrollY = 0.0f;
    active = NoItem;
    pressed = NoItem;
    pending = false;
    selection = {};
    opened = true;
    captured = true;
    draining = false;

    (void)reflow(viewport);
    moveSelection(1);
    return true;
}

void ContextMenuSystem::close() noexcept {
    if (!opened && !pending) {
        return;
    }

    hideSubmenu();
    opened = false;
    pending = false;
    selection = {};
    active = NoItem;
    pressed = NoItem;
    captured = true;
    draining = true;
}

bool ContextMenuSystem::reflow(Rect viewport) {
    if (!usableViewport(viewport)) {
        return false;
    }

    viewportRect = viewport;
    const Rect bounds = viewport.shrink(4.0f);
    const f32 width = std::min(preferredWidth, bounds.w);
    const f32 height = std::min(fullHeight, bounds.h);

    box = Rect{
        std::clamp(requestedPosition.x, bounds.x,
                   std::max(bounds.x, bounds.maxX() - width)),
        std::clamp(requestedPosition.y, bounds.y,
                   std::max(bounds.y, bounds.maxY() - height)),
        width, height};

    maxScroll = std::max(0.0f, fullHeight - height);
    scrollY = std::clamp(scrollY, 0.0f, maxScroll);
    return true;
}

Rect ContextMenuSystem::rowRect(usize index) const noexcept {
    return Rect{
        box.x + 4.0f,
        box.y + 4.0f + static_cast<f32>(index) * rowHeight - scrollY,
        box.w - 8.0f,
        rowHeight};
}

usize ContextMenuSystem::itemAt(Vec2 point) const noexcept {
    if (!box.shrink(4.0f).contains(point)) {
        return NoItem;
    }

    for (usize index = 0; index < items.size(); ++index) {
        if (rowRect(index).contains(point)) {
            return items[index].enabled ? index : NoItem;
        }
    }

    return NoItem;
}

void ContextMenuSystem::revealSelection() {
    if (active >= items.size()) {
        return;
    }

    const f32 top = static_cast<f32>(active) * rowHeight;
    const f32 bottom = top + rowHeight;
    const f32 visibleHeight = box.h - 8.0f;

    if (top < scrollY) {
        scrollY = top;
    } else if (bottom > scrollY + visibleHeight) {
        scrollY = bottom - visibleHeight;
    }

    scrollY = std::clamp(scrollY, 0.0f, maxScroll);
}

void ContextMenuSystem::moveSelection(int direction) {
    if (items.empty()) {
        active = NoItem;
        return;
    }

    usize index = active < items.size() ? active : (direction > 0 ? items.size() - 1 : 0);

    for (usize checked = 0; checked < items.size(); ++checked) {
        if (direction > 0) {
            index = index == items.size() - 1 ? 0 : index + 1;
        } else {
            index = index == 0 ? items.size() - 1 : index - 1;
        }

        if (items[index].enabled) {
            active = index;
            revealSelection();
            return;
        }
    }

    active = NoItem;
}

void ContextMenuSystem::activate(usize index) {
    if (index >= items.size() || !items[index].enabled) {
        return;
    }

    if (items[index].submenu) {
        showSubmenu(index, true);
        return;
    }

    const ContextMenuSelection result{owner, items[index].action};
    close();
    selection = result;
    pending = true;
}

void ContextMenuSystem::hideSubmenu() {
    if (expanded != NoItem) {
        items[expanded].submenu->close();
        expanded = NoItem;
    }
    submenuKeyboard = false;
}

void ContextMenuSystem::placeSubmenu() {
    if (expanded == NoItem) return;

    assert(expanded < items.size() && items[expanded].submenu);
    auto& child = *items[expanded].submenu;

    const Rect bounds = viewportRect.shrink(4.0f);
    const f32 width = std::min(child.preferredWidth, bounds.w);
    const f32 x = box.maxX() + width <= bounds.maxX() ? box.maxX() : box.x - width;

    child.requestedPosition = {x, rowRect(expanded).y - 4.0f};
    (void)child.reflow(viewportRect);
}

void ContextMenuSystem::showSubmenu(usize index, bool keyboard) {
    if (index >= items.size() || !items[index].enabled || !items[index].submenu) {
        return;
    }

    if (expanded != index) {
        hideSubmenu();
        expanded = index;

        auto& child = *items[index].submenu;
        child.opened = child.captured = true;
        child.draining = child.pending = false;
        child.selection = {};
        child.active = child.pressed = NoItem;
        child.scrollY = 0.0f;
    }

    submenuKeyboard = keyboard;
    placeSubmenu();

    auto& child = *items[index].submenu;
    if (keyboard && child.active == NoItem)
        child.moveSelection(1);
}

void ContextMenuSystem::update(const Tree& tree, NodeHandle currentRoot, Rect viewport, const InputState& input, Font* font)
{
    if ((opened || pending) && (currentRoot != root || !usableOwner(tree, root, owner))) {
        close();
        pending = false;
        selection = {};
    }

    captured = opened || draining;
    if (!captured) return;

    if (!opened) {
        if (!mouseHeld(input)) draining = false;
        return;
    }

    if (!font || !font->isLoaded() || !reflow(viewport)) {
        close();
        draining = mouseHeld(input);
        return;
    }

    placeSubmenu();

    const auto& mouse = input.mouse;
    const bool overChild = expanded != NoItem && items[expanded].submenu->box.contains(mouse.position);

    if (mouse.pressed(MouseButton::Right) || mouse.pressed(MouseButton::Middle) || (mouse.pressed(MouseButton::Left) &&
         !box.contains(mouse.position) && !overChild)) {
        close();
        draining = mouseHeld(input);
        return;
    }

    for (const auto& event : input.events) {
        if (event.type != InputEventType::Key ||
            event.action != KeyAction::Press) {
            continue;
        }

        if (event.key == Key::Tab || event.key == Key::Escape) {
            if (event.key == Key::Escape && expanded != NoItem)
                hideSubmenu();
            else
                close();

            draining = !opened && mouseHeld(input);
            return;
        }

        if (event.key == Key::Left && expanded != NoItem) {
            hideSubmenu();
            return;
        }
    }

    const bool parentPointer = !overChild && box.contains(mouse.position) && (mouse.moved() || mouse.pressed(MouseButton::Left) ||
         mouse.released(MouseButton::Left) || mouse.wheel.y != 0.0f);

    if (expanded != NoItem && (overChild || (submenuKeyboard && !parentPointer))) {
        submenuKeyboard = true;

        auto& child = *items[expanded].submenu;
        child.update(tree, currentRoot, viewport, input, font);

        ContextMenuSelection result;
        if (child.poll(tree, currentRoot, result)) {
            close();
            selection = result;
            pending = true;
        } else if (!child.opened) {
            hideSubmenu();
        }
        return;
    }

    if (parentPointer)
        submenuKeyboard = false;

    if (box.contains(mouse.position) &&
        std::isfinite(mouse.wheel.y) && mouse.wheel.y != 0.0f) {
        hideSubmenu();
        scrollY = std::clamp(scrollY - mouse.wheel.y * rowHeight * 3.0f,0.0f, maxScroll);
    }

    if ((mouse.moved() || mouse.wheel.y != 0.0f) && (box.contains(mouse.position) || expanded == NoItem)) {
        const usize hovered = itemAt(mouse.position);
        if (hovered != active)
            hideSubmenu();

        active = hovered;
        showSubmenu(active, false);
    }

    if (mouse.pressed(MouseButton::Left)) {
        pressed = itemAt(mouse.position);
        if (pressed != active)
            hideSubmenu();
        active = pressed;
    }

    if (mouse.released(MouseButton::Left)) {
        const usize released = itemAt(mouse.position);
        if (pressed != NoItem && pressed == released)
            activate(released);
        pressed = NoItem;
    }

    for (const auto& event : input.events) {
        if (!opened || submenuKeyboard)
            break;

        if (event.type != InputEventType::Key ||
            event.action == KeyAction::Release) {
            continue;
        }

        switch (event.key) {
        case Key::Down:
        case Key::Up:
            hideSubmenu();
            moveSelection(event.key == Key::Down ? 1 : -1);
            break;

        case Key::Home:
        case Key::End:
            hideSubmenu();
            active = NoItem;
            moveSelection(event.key == Key::Home ? 1 : -1);
            break;

        case Key::Right:
            showSubmenu(active, true);
            break;

        case Key::Enter:
        case Key::Space:
            if (event.action == KeyAction::Press)
                activate(active);
            break;

        default:
            break;
        }
    }

    if (!opened)
        draining = mouseHeld(input);
}

bool ContextMenuSystem::poll(const Tree& tree, NodeHandle currentRoot, ContextMenuSelection& out, NodeHandle filter) noexcept {
    if (!pending || (filter.isValid() && selection.owner != filter)) {
        return false;
    }

    pending = false;
    if (currentRoot != root || !usableOwner(tree, root, selection.owner)) {
        return false;
    }

    out = selection;
    return true;
}

void ContextMenuSystem::paint(const Tree& tree, NodeHandle currentRoot, Font* font, const Theme& theme, DrawList& list) const {
    if (!opened || !font || !font->isLoaded() ||
        currentRoot != root || !usableOwner(tree, root, owner)) {
        return;
    }

    list.pushClip(viewportRect);

    const ShadowStyle& shadow = theme.elevationMedium;
    list.addBoxShadow(box, shadow.color, theme.smallRadius,shadow.offset, shadow.blur, shadow.spread);
    list.addRoundedRectFilled(box, theme.surfaceRaised, theme.smallRadius);
    list.addRoundedRectOutline(box, theme.borderStrong, theme.smallRadius, theme.borderWidth);

    const Rect content = box.shrink(4.0f);
    list.pushClip(content);

    for (usize index = 0; index < items.size(); ++index) {
        const Rect row = rowRect(index);
        if (!row.intersects(content)) {
            continue;
        }

        const Item& item = items[index];
        if (index == active && item.enabled) {
            list.addRoundedRectFilled(
                row, theme.surfaceHovered, theme.smallRadius);
        }

        const Rect textClip{row.x + 8.0f, row.y, std::max(0.0f, row.w - (item.submenu ? 36.0f : 16.0f)),row.h};
        const Vec2 position{textClip.x, row.y + (rowHeight - font->metrics().lineHeight) * 0.5f};

        font->drawTextClipped(list, item.label, position, item.enabled ? theme.text : theme.textDisabled, textClip.intersection(content));

        if (item.submenu) {
            const f32 x = row.maxX() - 14.0f;
            const f32 y = row.y + row.h * 0.5f;
            const Color color = item.enabled ? theme.text : theme.textDisabled;

            list.addLine({x, y - 4.0f}, {x + 4.0f, y}, color, 1.5f);
            list.addLine({x + 4.0f, y}, {x, y + 4.0f}, color, 1.5f);
        }
    }

    list.popClip();

    if (maxScroll > 0.0f) {
        const f32 thumbHeight = std::min(
            content.h, std::max(8.0f, content.h * box.h / fullHeight));
        const f32 thumbY = content.y +
            (content.h - thumbHeight) * scrollY / maxScroll;

        list.addRoundedRectFilled(Rect{box.maxX() - 5.0f, thumbY, 2.0f, thumbHeight},theme.textMuted, 1.0f);
    }

    list.popClip();

    if (expanded != NoItem)
        items[expanded].submenu->paint(tree, currentRoot, font, theme, list);
}

} // namespace octogui