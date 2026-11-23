#include "editorPanels.hpp"
#include "editorMetrics.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
NodeHandle createTitledPanel(Context &ctx,
                            NodeHandle pane,
                            std::string_view icon,
                            std::string_view title,
                            IconButton *leadingAction)
{
    const NodeHandle panel = ctx.createPanel(pane);

    auto panelLayout = ctx.layoutStyle(panel);
    panelLayout.mode = LayoutMode::Vertical;
    panelLayout.horizontalAlignment = Alignment::Stretch;
    panelLayout.verticalAlignment = Alignment::Stretch;
    panelLayout.gap = SpaceS;
    panelLayout.padding = Padding{SpaceM};

    const NodeHandle header = ctx.createContainer(panel);

    auto headerLayout = ctx.layoutStyle(header);
    headerLayout.mode = LayoutMode::Horizontal;
    headerLayout.gap = SpaceS;
    headerLayout.verticalAlignment = Alignment::Center;
    headerLayout.minSize = Vec2{0.0f, PanelHeaderHeight};
    headerLayout.preferredSize = Vec2{0.0f, PanelHeaderHeight};
    headerLayout.maxSize = Vec2{InfiniteLayoutSize, PanelHeaderHeight};

    if (leadingAction)
    {
        headerLayout.minSize = Vec2{0.0f, PanelHeaderHeight};
        headerLayout.preferredSize = Vec2{0.0f, PanelHeaderHeight};
        headerLayout.maxSize = Vec2{InfiniteLayoutSize, PanelHeaderHeight};

        leadingAction->button = ctx.createButton(header, "");
        auto actionLayout = ctx.layoutStyle(leadingAction->button);
        actionLayout.mode = LayoutMode::Horizontal;
        actionLayout.horizontalAlignment = Alignment::Center;
        actionLayout.verticalAlignment = Alignment::Center;
        actionLayout.padding = Padding{};
        actionLayout.minSize = Vec2{IconButtonSize, IconButtonSize};
        actionLayout.preferredSize = Vec2{IconButtonSize, IconButtonSize};
        actionLayout.maxSize = Vec2{IconButtonSize, IconButtonSize};

        leadingAction->icon = ctx.createIcon(leadingAction->button, "plus");
        ctx.setIconSize(leadingAction->icon, IconSize);
        ctx.setIconColor(leadingAction->icon, ctx.theme().textMuted);
    }

    const NodeHandle headerIcon = ctx.createIcon(header, icon);
    ctx.setIconSize(headerIcon, IconSize);
    ctx.setIconColor(headerIcon, ctx.theme().textMuted);

    const NodeHandle heading = ctx.createLabel(header, title);
    ctx.setFontSlot(heading, FontSlot::Secondary);
    ctx.style(heading).all.text = ctx.theme().textMuted;

    return panel;
}

void styleSplitter(Context &ctx, NodeHandle split)
{
    // The 4 px gutter stays empty at rest; a 1 px teal line shows while the
    // divider is hovered or dragged.
    ctx.setSplitDividerLineWidth(split, 1.0f);
    auto style = ctx.style(split);
    style.hovered.indicator = ctx.theme().focus;
    style.active.indicator = ctx.theme().focus;
}
} // namespace Cthulhu::Editor
