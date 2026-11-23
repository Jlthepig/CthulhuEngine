#include <algorithm>
#include <limits>
#include <cassert>

#include "editorDialog.hpp"
#include "editorTheme.hpp"
#include "editorMetrics.hpp"

namespace Cthulhu::Editor
{
using namespace octogui;

void EditorDialog::build(Context &ctx, NodeHandle root, NodeHandle editorContent, float width)
{
    content = editorContent;
    backdrop = ctx.createPanel(root);
    ctx.setZLayer(backdrop, 1);
    auto backdropStyle = ctx.style(backdrop);
    backdropStyle.all.background = Color::fromRGBA8(0, 0, 0, 150);
    backdropStyle.borderWidth = 0.0f;
    backdropStyle.cornerRadius = 0.0f;
    backdropStyle.shadow = ShadowStyle{};

    auto backdropLayout = ctx.layoutStyle(backdrop);
    backdropLayout.mode = LayoutMode::Vertical;
    backdropLayout.horizontalAlignment = Alignment::Center;
    backdropLayout.verticalAlignment = Alignment::Center;
    backdropLayout.padding = Padding{SpaceL};
    backdropLayout.gap = 0.0f;
    backdropLayout.clip = true;
    backdropLayout.scrollY = true;

    const auto card = ctx.createPanel(backdrop);
    ctx.style(card).all.background = ctx.theme().surfaceRaised;
    ctx.style(card).shadow = ctx.theme().elevationHigh;
    ctx.style(card).cornerRadius = 6.0f;
    auto cardLayout = ctx.layoutStyle(card);
    cardLayout.preferredSize = Vec2{width, 0.0f};
    cardLayout.fitContentY = true;
    cardLayout.padding = Padding{SpaceXl};
    cardLayout.gap = SpaceL;
    cardLayout.clip = true;

    titleLabel = ctx.createLabel(card, "");
    ctx.setFontSlot(titleLabel, FontSlot::Secondary);
    messageLabel = ctx.createLabel(card, "");

    for (std::size_t i = 0; i < choices.size(); ++i)
    {
        choiceRows[i] = ctx.createContainer(card);
        auto row = ctx.layoutStyle(choiceRows[i]);
        row.mode = LayoutMode::Horizontal;
        row.preferredSize = Vec2{0.0f, ControlHeight};
        row.verticalAlignment = Alignment::Stretch;
        row.gap = SpaceM;

        choiceLabels[i] = ctx.createLabel(choiceRows[i], "");
        auto label = ctx.layoutStyle(choiceLabels[i]);
        label.minSize = Vec2{112.0f, ControlHeight};
        label.preferredSize = Vec2{112.0f, ControlHeight};
        label.maxSize = Vec2{112.0f, ControlHeight};

        choices[i] = ctx.createComboBox(choiceRows[i], {});
        ctx.layoutStyle(choices[i]).minSize = Vec2{0.0f, ControlHeight};
        ctx.layoutStyle(choices[i]).preferredSize = Vec2{480.0f, ControlHeight};
        ctx.layoutStyle(choices[i]).clip = true;

        ctx.setVisible(choices[i], false);
        ctx.setVisible(choiceRows[i], false);
    }

    details = ctx.createContainer(card);
    auto detailsLayout = ctx.layoutStyle(details);
    detailsLayout.mode = LayoutMode::Vertical;
    detailsLayout.horizontalAlignment = Alignment::Stretch;
    detailsLayout.padding = Padding{SpaceL, SpaceM, SpaceXl + SpaceM, SpaceM};
    detailsLayout.gap = SpaceS;
    detailsLayout.scrollY = true;
    detailsLayout.clip = true;
    auto detailsStyle = ctx.style(details);
    detailsStyle.all.background = ctx.theme().surface;
    detailsStyle.all.border = ctx.theme().border;
    detailsStyle.borderWidth = 1.0f;
    detailsStyle.cornerRadius = ctx.theme().controlRadius;
    ctx.setVisible(details, false);

    noticeLabel = ctx.createLabel(card, "");
    ctx.layoutStyle(noticeLabel).preferredSize = Vec2{0.0f, RowHeight};
    ctx.layoutStyle(noticeLabel).clip = true;
    ctx.style(noticeLabel).all.text = ctx.theme().textMuted;
    ctx.setVisible(noticeLabel, false);

    errorLabel = ctx.createTextInput(card, "");
    ctx.setTextInputReadOnly(errorLabel, true);
    auto errorStyle = ctx.style(errorLabel);
    errorStyle.all.background = Color::transparent();
    errorStyle.all.gradient = LinearGradient{};
    errorStyle.borderWidth = 0.0f;
    errorStyle.shadow = ShadowStyle{};
    ctx.layoutStyle(errorLabel).padding = Padding{0.0f, SpaceXs};
    for (const auto label : {titleLabel, messageLabel, errorLabel})
        ctx.layoutStyle(label).preferredSize = Vec2{0.0f, RowHeight};

    ctx.style(messageLabel).all.text = ctx.theme().textMuted;
    ctx.style(errorLabel).all.text = DangerColor;

    const auto row = ctx.createContainer(card);
    auto rowLayout = ctx.layoutStyle(row);
    rowLayout.mode = LayoutMode::Horizontal;
    rowLayout.horizontalAlignment = Alignment::End;
    rowLayout.preferredSize = Vec2{0.0f, DialogButtonHeight};
    rowLayout.verticalAlignment = Alignment::Stretch;
    rowLayout.gap = SpaceM;

    for (auto &button : buttons)
    {
        button = ctx.createButton(row, "");
        ctx.layoutStyle(button).minSize = Vec2{108.0f, DialogButtonHeight};
        ctx.layoutStyle(button).padding = Padding{SpaceXl, SpaceS};
    }
    ctx.setText(buttons.back(), "Cancel");

    ctx.setVisible(backdrop, false);
}

void EditorDialog::show(Context &ctx, std::string_view title, std::string_view message, std::string_view primary, std::string_view secondary, std::string_view cancel)
{
    if (!isOpen(ctx))
        previousFocus = ctx.focused();

    ctx.closeContextMenu();
    ctx.clearFocus();
    ctx.setText(titleLabel, title);
    ctx.setText(messageLabel, message);
    ctx.setTooltip(messageLabel, message);
    ctx.setText(buttons[0], primary);
    ctx.setVisible(buttons[0], !primary.empty());
    ctx.setText(buttons.back(), cancel);
    ctx.setText(buttons[1], secondary);
    ctx.setVisible(buttons[1], !secondary.empty());
    ctx.setEnabled(buttons[0], true);

    for (std::size_t i = 0; i < choices.size(); ++i)
        setChoices(ctx, i, {}, {});

    setDetails(ctx, {});
    setNotice(ctx, {});
    setError(ctx, {});
    ctx.setEnabled(content, false);
    ctx.setVisible(backdrop, true);
    ctx.setScrollOffsetY(backdrop, 0.0f);
    ctx.setFocused(buttons.back());
}

void EditorDialog::setChoices(Context &ctx, std::size_t index, std::string_view label, const std::vector<std::string> &items)
{
    assert(index < choices.size());
    if (index >= choices.size())
        return;

    ctx.setText(choiceLabels[index], label);
    ctx.setComboBoxItems(choices[index], items);
    ctx.setComboBoxSelectedIndex(choices[index], items.empty() ? -1 : 0);
    ctx.setVisible(choiceRows[index], !items.empty());
    ctx.setVisible(choices[index], !items.empty());
    ctx.setEnabled(choices[index], !items.empty());
}

i32 EditorDialog::selectedChoice(const Context &ctx, std::size_t index) const
{
    assert(index < choices.size());
    return index < choices.size() ? ctx.comboBoxSelectedIndex(choices[index]) : -1;
}

void EditorDialog::setDetails(Context &ctx, std::span<const std::string> lines)
{
    for (const auto row : detailRows)
        ctx.destroyNode(row);
    detailRows.clear();
    detailRows.reserve(lines.size());

    for (const auto &line : lines)
    {
        const auto row = ctx.createTextInput(details, line);
        ctx.setTextInputReadOnly(row, true);
        ctx.setTooltip(row, line);

        auto style = ctx.style(row);
        style.all.background = Color::transparent();
        style.all.gradient = LinearGradient{};
        style.borderWidth = 0.0f;
        style.cornerRadius = 0.0f;
        style.shadow = ShadowStyle{};
        style.all.text = detailRows.empty() ? ctx.theme().text : ctx.theme().textMuted;

        auto layout = ctx.layoutStyle(row);
        layout.padding = Padding{0.0f, SpaceXs};
        layout.minSize = Vec2{0.0f, RowHeight};
        layout.preferredSize = Vec2{0.0f, RowHeight};
        layout.maxSize = Vec2{InfiniteLayoutSize, RowHeight};
        detailRows.push_back(row);
    }

    const float height = 2.0f * SpaceM + (RowHeight + SpaceS) * static_cast<float>(std::min<std::size_t>(lines.size(), 8)) - SpaceS;
    auto layout = ctx.layoutStyle(details);
    layout.minSize = Vec2{0.0f, height};
    layout.preferredSize = Vec2{0.0f, height};
    layout.maxSize = Vec2{InfiniteLayoutSize, height};
    ctx.setVisible(details, !lines.empty());
    ctx.setScrollOffsetY(details, 0.0f);
}

void EditorDialog::setNotice(Context &ctx, std::string_view message)
{
    ctx.setText(noticeLabel, message);
    ctx.setTooltip(noticeLabel, message);
    ctx.setVisible(noticeLabel, !message.empty());
}

void EditorDialog::setPrimaryEnabled(Context &ctx, bool enabled)
{
    ctx.setEnabled(buttons[0], enabled);
    if (!enabled && ctx.isFocused(buttons[0]))
        ctx.setFocused(buttons.back());
}

void EditorDialog::hide(Context &ctx)
{
    ctx.clearFocus();
    ctx.setVisible(backdrop, false);
    ctx.setEnabled(content, true);

    if (ctx.isValid(previousFocus) && ctx.isVisible(previousFocus) &&
        ctx.isEnabled(previousFocus))
        ctx.setFocused(previousFocus);

    previousFocus = {};
}

void EditorDialog::setError(Context &ctx, std::string_view message)
{
    ctx.setText(errorLabel, message);
    ctx.setTooltip(errorLabel, message);
    ctx.setVisible(errorLabel, !message.empty());
}

DialogChoice EditorDialog::pollChoice(Context &ctx) const
{
    if (!isOpen(ctx) || ctx.contextMenuOwnsInput())
        return DialogChoice::None;

    constexpr DialogChoice buttonChoices[] = {
        DialogChoice::Primary,
        DialogChoice::Secondary,
        DialogChoice::Cancel
    };

    const auto available = [&](NodeHandle node) {
        return ctx.isVisible(node) && ctx.isEnabled(node);
    };

    for (std::size_t i = 0; i < buttons.size(); ++i)
        if (available(buttons[i]) && ctx.isClicked(buttons[i]))
            return buttonChoices[i];

    const std::size_t count = choices.size() + detailRows.size() + 1 + buttons.size();

    const auto control = [&](std::size_t index) {
        if (index < choices.size())
            return choices[index];

        index -= choices.size();
        if (index < detailRows.size())
            return detailRows[index];
        if (index == detailRows.size())
            return errorLabel;

        return buttons[index - detailRows.size() - 1];
    };

    // cancel stays available so keyboard traversal always terminates
    assert(available(buttons.back()));

    std::size_t focused = count - 1;
    for (std::size_t i = 0; i < count; ++i)
        if (available(control(i)) && ctx.isFocused(control(i)))
            focused = i;

    if (!ctx.isFocused(control(focused)))
        ctx.setFocused(control(focused));

    for (const auto &event : ctx.input().events)
    {
        if (event.type != InputEventType::Key ||
            event.action != KeyAction::Press)
            continue;

        if (event.key == Key::Escape)
            return DialogChoice::Cancel;

        if (event.key == Key::Tab)
        {
            do
            {
                focused = (focused + (event.modifiers.shift ? count - 1 : 1) ) % count;
            } while (!available(control(focused)));

            ctx.setFocused(control(focused));

            const bool detailFocused = focused >= choices.size() && focused < choices.size() + detailRows.size();

            if (detailFocused)
            {
                const Rect row = ctx.tree().rect(control(focused));
                const Rect area = ctx.tree().rect(details).shrink(SpaceM);
                float offset = ctx.scrollOffsetY(details);

                if (row.y - offset < area.y)
                    offset = row.y - area.y;
                else if (row.maxY() - offset > area.maxY())
                    offset = row.maxY() - area.maxY();

                ctx.setScrollOffsetY(details, offset);
            }

            const Rect row = ctx.tree().rect(control(focused));
            const Rect area = ctx.tree().rect(backdrop);
            const float inner = detailFocused ? ctx.scrollOffsetY(details) : 0.0f;
            float offset = ctx.scrollOffsetY(backdrop);

            if (row.y - inner - offset < area.y)
                offset = row.y - inner - area.y;
            else if (row.maxY() - inner - offset > area.maxY())
                offset = row.maxY() - inner - area.maxY();

            ctx.setScrollOffsetY(backdrop, offset);
        }
        else if (focused < choices.size() &&
                 (event.key == Key::Up || event.key == Key::Down ||
                  event.key == Key::Home || event.key == Key::End))
        {
            i32 selected = ctx.comboBoxSelectedIndex(choices[focused]);

            if (event.key == Key::Home)
                selected = 0;
            else if (event.key == Key::End)
                selected = std::numeric_limits<i32>::max();
            else if (event.key == Key::Up && selected > 0)
                --selected;
            else if (event.key == Key::Down && selected < std::numeric_limits<i32>::max())
                ++selected;

            ctx.setComboBoxSelectedIndex(choices[focused], selected);
        }
        else if (event.key == Key::PageUp || event.key == Key::PageDown)
        {
            const auto target = ctx.isVisible(details) ? details : backdrop;
            const float page = ctx.tree().rect(target).h;
            ctx.setScrollOffsetY(target, ctx.scrollOffsetY(target) + (event.key == Key::PageUp ? -page : page));
        }
        else if ((event.key == Key::Enter || event.key == Key::Space) && !event.modifiers.ctrl && !event.modifiers.alt && !event.modifiers.shift)
        {
            if (focused < choices.size())
            {
                ctx.clearFocus();
                ctx.setFocused(choices[focused]);
                continue;
            }

            for (std::size_t i = 0; i < buttons.size(); ++i)
                if (available(buttons[i]) && ctx.isFocused(buttons[i]))
                    return buttonChoices[i];
        }
    }

    return DialogChoice::None;
}
} // namespace Cthulhu::Editor
