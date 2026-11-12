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

    const auto card = ctx.createPanel(backdrop);
    ctx.style(card).all.background = ctx.theme().surfaceRaised;
    auto cardLayout = ctx.layoutStyle(card);
    cardLayout.preferredSize = Vec2{528.0f, 0.0f};
    cardLayout.fitContentY = true;
    cardLayout.padding = Padding{16.0f};
    cardLayout.gap = 8.0f;
    cardLayout.clip = true;

    titleLabel = ctx.createLabel(card, "");
    messageLabel = ctx.createLabel(card, "");
    errorLabel = ctx.createLabel(card, "");
    for (const auto label : {titleLabel, messageLabel, errorLabel})
        ctx.layoutStyle(label).preferredSize = Vec2{0.0f, 28.0f};

    ctx.style(messageLabel).all.text = ctx.theme().textMuted;
    ctx.style(errorLabel).all.text = Color::fromRGBA8(242, 115, 115);

    const auto row = ctx.createContainer(card);
    auto rowLayout = ctx.layoutStyle(row);
    rowLayout.mode = LayoutMode::Horizontal;
    rowLayout.preferredSize = Vec2{0.0f, 36.0f};
    rowLayout.verticalAlignment = Alignment::Stretch;
    rowLayout.gap = 8.0f;

    for (auto &button : buttons)
        button = ctx.createButton(row, "");
    ctx.setText(buttons.back(), "Cancel");

    ctx.setVisible(backdrop, false);
}

void EditorDialog::show(Context &ctx, std::string_view title, std::string_view message, std::string_view primary, std::string_view secondary)
{
    ctx.setText(titleLabel, title);
    ctx.setText(messageLabel, message);
    ctx.setTooltip(messageLabel, message);
    ctx.setText(buttons[0], primary);
    ctx.setText(buttons[1], secondary);
    setError(ctx, {});
    ctx.setEnabled(content, false);
    ctx.setVisible(backdrop, true);
    ctx.setFocused(buttons.back());
}

void EditorDialog::hide(Context &ctx)
{
    ctx.clearFocus();
    ctx.setVisible(backdrop, false);
    ctx.setEnabled(content, true);
}

void EditorDialog::setError(Context &ctx, std::string_view message)
{
    ctx.setText(errorLabel, message);
    ctx.setTooltip(errorLabel, message);
    ctx.setVisible(errorLabel, !message.empty());
}

DialogChoice EditorDialog::pollChoice(Context &ctx) const
{
    if (ctx.contextMenuOwnsInput())
        return DialogChoice::None;

    constexpr DialogChoice choices[] = {DialogChoice::Primary, DialogChoice::Secondary, DialogChoice::Cancel};
    std::size_t focused = buttons.size() - 1;
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        if (ctx.isClicked(buttons[i]))
            return choices[i];
        if (ctx.isFocused(buttons[i]))
            focused = i;
    }

    if (!ctx.isFocused(buttons[focused]))
        ctx.setFocused(buttons[focused]);

    for (const auto &event : ctx.input().events)
    {
        if (event.type != InputEventType::Key || event.action != KeyAction::Press)
            continue;

        if (event.key == Key::Escape)
            return DialogChoice::Cancel;

        if (event.key == Key::Tab)
        {
            const auto count = buttons.size();
            focused = (focused + (event.modifiers.shift ? count - 1 : 1)) % count;
            ctx.setFocused(buttons[focused]);
        }
        else if (event.key == Key::Enter || event.key == Key::Space)
        {
            return choices[focused];
        }
    }

    return DialogChoice::None;
}
} // namespace Cthulhu::Editor