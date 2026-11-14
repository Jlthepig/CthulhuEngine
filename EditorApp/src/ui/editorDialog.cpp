#include <algorithm>
#include <cassert>

#include "editorDialog.hpp"

namespace Cthulhu::Editor
{
using namespace octogui;

void EditorDialog::build(Context &ctx, NodeHandle root, NodeHandle editorContent)
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
    backdropLayout.padding = Padding{12.0f};
    backdropLayout.gap = 0.0f;
    backdropLayout.clip = true;
    backdropLayout.scrollY = true;

    const auto card = ctx.createPanel(backdrop);
    ctx.style(card).all.background = ctx.theme().surfaceRaised;
    auto cardLayout = ctx.layoutStyle(card);
    cardLayout.preferredSize = Vec2{680.0f, 0.0f};
    cardLayout.fitContentY = true;
    cardLayout.padding = Padding{20.0f};
    cardLayout.gap = 12.0f;
    cardLayout.clip = true;

    titleLabel = ctx.createLabel(card, "");
    messageLabel = ctx.createLabel(card, "");
    details = ctx.createContainer(card);
    auto detailsLayout = ctx.layoutStyle(details);
    detailsLayout.mode = LayoutMode::Vertical;
    detailsLayout.horizontalAlignment = Alignment::Stretch;
    detailsLayout.padding = Padding{12.0f, 10.0f, 24.0f, 10.0f};
    detailsLayout.gap = 4.0f;
    detailsLayout.scrollY = true;
    detailsLayout.clip = true;
    auto detailsStyle = ctx.style(details);
    detailsStyle.all.background = ctx.theme().surface;
    detailsStyle.all.border = ctx.theme().border;
    detailsStyle.borderWidth = 1.0f;
    detailsStyle.cornerRadius = 4.0f;
    ctx.setVisible(details, false);

    noticeLabel = ctx.createLabel(card, "");
    ctx.layoutStyle(noticeLabel).preferredSize = Vec2{0.0f, 24.0f};
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
    ctx.layoutStyle(errorLabel).padding = Padding{0.0f, 2.0f};
    for (const auto label : {titleLabel, messageLabel, errorLabel})
        ctx.layoutStyle(label).preferredSize = Vec2{0.0f, 28.0f};

    ctx.style(messageLabel).all.text = ctx.theme().textMuted;
    ctx.style(errorLabel).all.text = Color::fromRGBA8(242, 115, 115);

    const auto row = ctx.createContainer(card);
    auto rowLayout = ctx.layoutStyle(row);
    rowLayout.mode = LayoutMode::Horizontal;
    rowLayout.horizontalAlignment = Alignment::End;
    rowLayout.preferredSize = Vec2{0.0f, 36.0f};
    rowLayout.verticalAlignment = Alignment::Stretch;
    rowLayout.gap = 8.0f;

    for (auto &button : buttons)
    {
        button = ctx.createButton(row, "");
        ctx.layoutStyle(button).minSize = Vec2{108.0f, 36.0f};
        ctx.layoutStyle(button).padding = Padding{16.0f, 4.0f};
    }
    ctx.setText(buttons.back(), "Cancel");

    ctx.setVisible(backdrop, false);
}

void EditorDialog::show(Context &ctx, std::string_view title, std::string_view message, std::string_view primary, std::string_view secondary)
{
    if (!isOpen(ctx))
        previousFocus = ctx.focused();

    ctx.closeContextMenu();
    ctx.clearFocus();
    ctx.setText(titleLabel, title);
    ctx.setText(messageLabel, message);
    ctx.setTooltip(messageLabel, message);
    ctx.setText(buttons[0], primary);
    ctx.setText(buttons[1], secondary);
    ctx.setVisible(buttons[1], !secondary.empty());
    ctx.setEnabled(buttons[0], true);
    setDetails(ctx, {});
    setNotice(ctx, {});
    setError(ctx, {});
    ctx.setEnabled(content, false);
    ctx.setVisible(backdrop, true);
    ctx.setScrollOffsetY(backdrop, 0.0f);
    ctx.setFocused(buttons.back());
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
        layout.padding = Padding{0.0f, 2.0f};
        layout.minSize = Vec2{0.0f, 28.0f};
        layout.preferredSize = Vec2{0.0f, 28.0f};
        layout.maxSize = Vec2{InfiniteLayoutSize, 28.0f};
        detailRows.push_back(row);
    }

    const float height = 20.0f + 32.0f * static_cast<float>(std::min<std::size_t>(lines.size(), 8)) - 4.0f;
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

    constexpr DialogChoice choices[] = {DialogChoice::Primary, DialogChoice::Secondary, DialogChoice::Cancel};

    const auto available = [&](NodeHandle node) { return ctx.isVisible(node) && ctx.isEnabled(node); };

    for (std::size_t i = 0; i < buttons.size(); ++i)
        if (available(buttons[i]) && ctx.isClicked(buttons[i]))
            return choices[i];

    const std::size_t count = detailRows.size() + 1 + buttons.size();
    const auto control = [&](std::size_t index) {
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
        if (event.type != InputEventType::Key || event.action != KeyAction::Press)
            continue;

        if (event.key == Key::Escape)
            return DialogChoice::Cancel;

        if (event.key == Key::Tab)
        {
            do
            {
                focused = (focused + (event.modifiers.shift ? count - 1 : 1)) % count;
            } while (!available(control(focused)));

            ctx.setFocused(control(focused));

            if (focused < detailRows.size())
            {
                const Rect row = ctx.tree().rect(control(focused));
                const Rect area = ctx.tree().rect(details).shrink(10.0f);
                float offset = ctx.scrollOffsetY(details);

                if (row.y - offset < area.y)
                    offset = row.y - area.y;
                else if (row.maxY() - offset > area.maxY())
                    offset = row.maxY() - area.maxY();

                ctx.setScrollOffsetY(details, offset);
            }

            const Rect row = ctx.tree().rect(control(focused));
            const Rect area = ctx.tree().rect(backdrop);
            const float inner = focused < detailRows.size() ? ctx.scrollOffsetY(details) : 0.0f;
            float offset = ctx.scrollOffsetY(backdrop);

            if (row.y - inner - offset < area.y)
                offset = row.y - inner - area.y;
            else if (row.maxY() - inner - offset > area.maxY())
                offset = row.maxY() - inner - area.maxY();

            ctx.setScrollOffsetY(backdrop, offset);
        }
        else if (event.key == Key::PageUp || event.key == Key::PageDown)
        {
            const auto target = ctx.isVisible(details) ? details : backdrop;
            const float page = ctx.tree().rect(target).h;
            ctx.setScrollOffsetY(target, ctx.scrollOffsetY(target) + (event.key == Key::PageUp ? -page : page));
        }
        else if ((event.key == Key::Enter || event.key == Key::Space) && !event.modifiers.ctrl && !event.modifiers.alt &&  !event.modifiers.shift)
        {
            for (std::size_t i = 0; i < buttons.size(); ++i)
                if (available(buttons[i]) && ctx.isFocused(buttons[i]))
                    return choices[i];
        }
    }

    return DialogChoice::None;
}
} // namespace Cthulhu::Editor
