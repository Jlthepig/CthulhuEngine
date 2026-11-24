#include "editorPanels.hpp"
#include "editorMetrics.hpp"
#include "editorTheme.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
NodeHandle createTitledPanel(Context &ctx,
                            NodeHandle pane,
                            std::string_view icon,
                            std::string_view title,
                            IconButton *action)
{
    const NodeHandle panel = ctx.createPanel(pane);

    auto panelLayout = ctx.layoutStyle(panel);
    panelLayout.mode = LayoutMode::Vertical;
    panelLayout.horizontalAlignment = Alignment::Stretch;
    panelLayout.verticalAlignment = Alignment::Stretch;
    panelLayout.gap = SpaceS;
    panelLayout.padding = Padding{SpaceM};
    ctx.style(panel).shadow = PanelShadow;
    ctx.style(panel).all.border = ctx.theme().border;
    ctx.style(panel).borderWidth = 1.0f;

    // Header << icon and title on the left, the optional action on the right.
    const NodeHandle header = ctx.createContainer(panel);

    auto headerLayout = ctx.layoutStyle(header);
    headerLayout.mode = LayoutMode::Horizontal;
    headerLayout.gap = SpaceS;
    headerLayout.verticalAlignment = Alignment::Center;
    headerLayout.minSize = Vec2{0.0f, PanelHeaderHeight};
    headerLayout.preferredSize = Vec2{0.0f, PanelHeaderHeight};
    headerLayout.maxSize = Vec2{InfiniteLayoutSize, PanelHeaderHeight};

    const NodeHandle headerIcon = ctx.createIcon(header, icon);
    ctx.setIconSize(headerIcon, IconSize);
    ctx.setIconColor(headerIcon, ctx.theme().textMuted);

    const NodeHandle heading = ctx.createLabel(header, title);
    ctx.setFontSlot(heading, FontSlot::Secondary);
    ctx.style(heading).all.text = ctx.theme().textMuted;

    if (action)
    {
        (void)ctx.createContainer(header);

        action->button = ctx.createButton(header, "");
        auto actionLayout = ctx.layoutStyle(action->button);
        actionLayout.mode = LayoutMode::Horizontal;
        actionLayout.horizontalAlignment = Alignment::Center;
        actionLayout.verticalAlignment = Alignment::Center;
        actionLayout.padding = Padding{};
        actionLayout.minSize = Vec2{IconButtonSize, IconButtonSize};
        actionLayout.preferredSize = Vec2{IconButtonSize, IconButtonSize};
        actionLayout.maxSize = Vec2{IconButtonSize, IconButtonSize};
        styleGhostButton(ctx, action->button);

        action->icon = ctx.createIcon(action->button, "plus");
        ctx.setIconSize(action->icon, IconSize);
        ctx.setIconColor(action->icon, ctx.theme().textMuted);
    }

    return panel;
}

void styleGhostButton(Context &ctx, NodeHandle button)
{
    auto style = ctx.style(button);
    style.all.background = Color::transparent();
    style.borderWidth = 0.0f;
    style.hovered.background = ctx.theme().surfaceHovered;
    style.active.background = ctx.theme().surfaceActive;
}

void styleButtonRole(Context &ctx, NodeHandle button, ButtonRole role)
{
    const Theme &theme = ctx.theme();
    Color background = theme.surfaceRaised;
    Color hovered = theme.surfaceHovered;
    Color pressed = theme.surfaceActive;
    Color text = theme.text;
    Color border = theme.border;
    float borderWidth = theme.borderWidth;

    switch (role)
    {
    case ButtonRole::Neutral:
        break;
    case ButtonRole::Primary:
        background = theme.accent;
        hovered = theme.accentHovered;
        pressed = theme.accentActive;
        text = theme.accentText;
        border = Color::transparent();
        borderWidth = 0.0f;
        break;
    case ButtonRole::Destructive:
        background = DangerColor.withAlpha(0.16f);
        hovered = DangerColor.withAlpha(0.26f);
        pressed = DangerColor.withAlpha(0.34f);
        text = DangerColor;
        border = DangerColor.withAlpha(0.45f);
        break;
    case ButtonRole::Ghost:
        background = Color::transparent();
        border = Color::transparent();
        borderWidth = 0.0f;
        break;
    }

    auto style = ctx.style(button);
    style.borderWidth = borderWidth;
    style.normal.background = background;
    style.normal.text = text;
    style.normal.border = border;
    style.focused.background = background;
    style.focused.text = text;
    style.focused.border = border;
    style.hovered.background = hovered;
    style.hovered.text = text;
    style.hovered.border = border;
    style.active.background = pressed;
    style.active.text = text;
    style.active.border = border;
}

void styleSplitter(Context &ctx, NodeHandle split)
{
    ctx.setSplitDividerLineWidth(split, 1.0f);
    auto style = ctx.style(split);
    style.hovered.indicator = ctx.theme().focus;
    style.active.indicator = ctx.theme().focus;
}
} // namespace Cthulhu::Editor
