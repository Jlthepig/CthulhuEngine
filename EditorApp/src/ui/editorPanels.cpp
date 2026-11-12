#include "editorPanels.hpp"

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
    panelLayout.gap = 6.0f;
    panelLayout.padding = Padding{8.0f};

    const NodeHandle header = ctx.createContainer(panel);

    auto headerLayout = ctx.layoutStyle(header);
    headerLayout.mode = LayoutMode::Horizontal;
    headerLayout.gap = 6.0f;
    headerLayout.verticalAlignment = Alignment::Center;
    headerLayout.minSize = Vec2{0.0f, 20.0f};
    headerLayout.preferredSize = Vec2{0.0f, 20.0f};
    headerLayout.maxSize = Vec2{InfiniteLayoutSize, 20.0f};

    if (leadingAction)
    {
        headerLayout.minSize = Vec2{0.0f, 26.0f};
        headerLayout.preferredSize = Vec2{0.0f, 26.0f};
        headerLayout.maxSize = Vec2{InfiniteLayoutSize, 26.0f};

        leadingAction->button = ctx.createButton(header, "");
        auto actionLayout = ctx.layoutStyle(leadingAction->button);
        actionLayout.mode = LayoutMode::Horizontal;
        actionLayout.horizontalAlignment = Alignment::Center;
        actionLayout.verticalAlignment = Alignment::Center;
        actionLayout.padding = Padding{};
        actionLayout.minSize = Vec2{24.0f, 24.0f};
        actionLayout.preferredSize = Vec2{24.0f, 24.0f};
        actionLayout.maxSize = Vec2{24.0f, 24.0f};

        leadingAction->icon = ctx.createIcon(leadingAction->button, "plus");
        ctx.setIconSize(leadingAction->icon, 16.0f);
        ctx.setIconColor(leadingAction->icon, ctx.theme().textMuted);
    }

    const NodeHandle headerIcon = ctx.createIcon(header, icon);
    ctx.setIconSize(headerIcon, 16.0f);
    ctx.setIconColor(headerIcon, ctx.theme().textMuted);

    const NodeHandle heading = ctx.createLabel(header, title);
    ctx.style(heading).all.text = ctx.theme().textMuted;

    return panel;
}
} // namespace Cthulhu::Editor
