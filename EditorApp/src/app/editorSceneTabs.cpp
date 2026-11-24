#include "editorShell.hpp"
#include "editorMetrics.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "engine.hpp"
#include "input.hpp"
#include "session.hpp"

namespace Cthulhu::Editor
{
namespace
{
constexpr float TabMinWidth = 96.0f;
constexpr float TabMaxWidth = 220.0f;
constexpr float TabHeight = PanelHeaderHeight;
constexpr float TabGap = 1.0f;
constexpr float ArrowWidth = IconButtonSize;
constexpr std::string_view DirtyDot = "\xE2\x80\xA2";
constexpr Color ActiveTabHovered = Color::fromRGBA8(37, 43, 43);
}

void styleTabButton(Context &ctx, NodeHandle button)
{
    auto style = ctx.style(button);
    style.all.background = Color::transparent();
    style.all.gradient = LinearGradient{};
    style.borderWidth = 0.0f;
    style.focusRing = FocusRingStyle{};
    style.pressedOffsetY = 0.0f;
    style.hovered.background = ctx.theme().surfaceHovered;
    style.active.background = ctx.theme().surfaceActive;
}

NodeHandle tabIconButton(Context &ctx, NodeHandle parent, std::string_view icon, std::string_view tooltip,
                         NodeHandle *iconOut = nullptr)
{
    const auto button = ctx.createButton(parent,"");
    auto layout = ctx.layoutStyle(button);
    layout.mode = LayoutMode::Horizontal;
    layout.minSize = {ArrowWidth, TabHeight};
    layout.preferredSize = {ArrowWidth, TabHeight};
    layout.maxSize = {ArrowWidth, TabHeight};
    layout.horizontalAlignment = Alignment::Center;
    layout.verticalAlignment = Alignment::Center;
    styleTabButton(ctx, button);

    const auto image = ctx.createIcon(button, icon);
    ctx.setIconSize(image, IconSize);
    ctx.setTooltip(button, tooltip);
    if (iconOut)
        *iconOut = image;
    return button;
}

void EditorShell::buildSceneTabBar(Context &ctx, NodeHandle parent)
{
    tabBar = ctx.createContainer(parent);
    auto bar = ctx.layoutStyle(tabBar);
    bar.mode = LayoutMode::Horizontal;
    bar.preferredSize = {0.0f, TabHeight};
    bar.gap = TabGap;
    bar.verticalAlignment = Alignment::Stretch;
    bar.clip = true;
    ctx.style(tabBar).all.background = ctx.theme().surface;

    previousTabs = tabIconButton(ctx, tabBar, "chevron-left", "Previous scene tabs");
    tabRow = ctx.createContainer(tabBar);
    nextTabs = tabIconButton(ctx, tabBar, "chevron-right", "Next scene tabs");
    ctx.setVisible(previousTabs, false);
    ctx.setVisible(nextTabs, false);
    
    auto row = ctx.layoutStyle(tabRow);
    row.mode = LayoutMode::Horizontal;
    row.preferredSize = {0.0f, TabHeight};
    row.verticalAlignment = Alignment::Stretch;
    row.gap = TabGap;
    row.clip = true;
}

void EditorShell::addSceneTab(Context &ctx)
{
    auto &tab = sceneTabs.emplace_back();
    tab.root = ctx.createContainer(tabRow);
    auto layout = ctx.layoutStyle(tab.root);
    layout.mode = LayoutMode::Horizontal;
    layout.preferredSize = {tab.width, TabHeight};
    layout.verticalAlignment = Alignment::Stretch;
    ctx.style(tab.root).cornerRadius = 0.0f;

    tab.button = ctx.createButton(tab.root, "");
    ctx.setTextAlign(tab.button, TextAlign::Start, TextAlign::Center);
    auto title = ctx.layoutStyle(tab.button);
    title.minSize = {tab.width - ArrowWidth, TabHeight};
    title.maxSize = {tab.width - ArrowWidth, TabHeight};
    title.padding = Padding{SpaceM, 0.0f, SpaceXs, 0.0f};
    title.clip = true;
    styleTabButton(ctx, tab.button);
    tab.close = tabIconButton(ctx, tab.root, "close", "Close scene", &tab.closeIcon);
    for (const auto node : {tab.button, tab.close})
    {
        ctx.style(node).hovered.background = Color::transparent();
        ctx.style(node).active.background = Color::transparent();
    }
    ctx.style(tab.button).hovered.text = ctx.theme().text;
}

void EditorShell::rememberSceneView(Engine &engine)
{
    if (sceneTabs.empty())
        return;

    auto &tab = sceneTabs[activeTab];
    assert(!tab.document);
    if (!restoreTabSelection)
        tab.selection = hierarchyPanelUI.selection();
    if (const auto *camera = engine.getCamera())
        tab.camera = *camera;
}

void EditorShell::restoreSceneView(Engine &engine)
{
    if (sceneTabs.empty())
        return;

    const auto &tab = sceneTabs[activeTab];
    if (auto *camera = engine.getCamera(); camera && tab.camera)
        *camera = *tab.camera;

    restoreTabSelection = true;
    Core::Input::resetMouseTracking();
    (void)Core::Input::getScrollDeltaY();
}

Scribe::Result EditorShell::selectSceneTab(Engine &engine, Scribe::Session &session, std::size_t index, bool keepCurrent)
{
    assert(index < sceneTabs.size());
    if (index == activeTab)
        return Scribe::Result::noChange();
    if (!engine.hasActiveScene())
        return Scribe::Result::failed("NO ACTIVE SCENE TO SWITCH FROM");

    if (keepCurrent)
        rememberSceneView(engine);
    auto switched = session.activateSceneDocument(sceneTabs[index].document, keepCurrent);
    if (!switched.result.ok())
        return switched.result;

    assert((switched.previous != nullptr) == keepCurrent);
    sceneTabs[activeTab].document = switched.previous;
    activeTab = index;
    restoreTabSelection = true;
    if (auto *camera = engine.getCamera(); camera && sceneTabs[index].camera)
        *camera = *sceneTabs[index].camera;

    Core::Input::resetMouseTracking();
    return Scribe::Result::applied();
}

Scribe::Result EditorShell::closeSceneTab(Context &ctx, Engine &engine, Scribe::Session &session,std::size_t index, std::size_t returnTab)
{
    assert(index < sceneTabs.size() && returnTab < sceneTabs.size());
    const bool closingActive = index == activeTab;
    const auto *scene = engine.getActiveScene();
    // Keep the final untouched placeholder instead of destroying and recreating it.
    if (closingActive && sceneTabs.size() == 1 && scene && !scene->hasResourcePath() &&
        !scene->isDirty() && scene->getEntityCount() == 0)
        return Scribe::Result::noChange();

    if (closingActive && sceneTabs.size() > 1)
    {
        const auto neighbor = index + 1 < sceneTabs.size() ? index + 1 : index - 1;
        const auto replacement = returnTab == index ? neighbor : returnTab;
        const auto result = selectSceneTab(engine, session, replacement, false);
        if (!result.ok())
            return result;
    }
    else
    {
        const auto result = closingActive ? session.closeActiveSceneDocument() : session.closeInactiveSceneDocument(sceneTabs[index].document);
        if (!result.ok())
            return result;
    }

    ctx.destroyNode(sceneTabs[index].root);
    sceneTabs.erase(sceneTabs.begin() + index);
    if (index < activeTab)
        --activeTab;
    if (index < firstVisibleTab)
        --firstVisibleTab;

    if (sceneTabs.empty())
    {
        activeTab = firstVisibleTab = 0;
        restoreTabSelection = false;
    }
    else if (closingActive && !ctx.isVisible(sceneTabs[activeTab].root))
    {
        firstVisibleTab = activeTab;
    }

    return Scribe::Result::applied();
}

void EditorShell::requestSceneTabClose(Context &ctx, Engine &engine, Scribe::Session &session, std::size_t index)
{
    assert(index < sceneTabs.size());

    auto dirtyName = engine.unsavedEditSceneName();
    if (const auto *doc = sceneTabs[index].document)
    {
        dirtyName.reset();
        if (doc->snapshot.dirty)
            dirtyName = doc->snapshot.name;
    }

    if (dirtyName)
    {
        closingTab = index;
        closeReturnTab = activeTab;
        exitDialog.show(ctx, "Save changes before closing?", dirtyName->empty() ? "Untitled" : *dirtyName, "Save", "Discard Changes", "Cancel", ButtonRole::Primary, ButtonRole::Destructive);
    }
    else
    {
        const auto result = closeSceneTab(ctx, engine, session, index, activeTab);
        contentBrowserUI.reportError(ctx, result.error);
    }
}

void EditorShell::handleSceneTabClose(Context &ctx, Engine &engine, Scribe::Session &session, DialogChoice choice)
{
    if (choice == DialogChoice::None)
        return;

    const auto index = *closingTab;
    if (choice == DialogChoice::Primary)
    {
        auto result = selectSceneTab(engine, session, index);
        if (result.ok())
            result = saveScene(ctx, engine, session);
        if (result.status != Scribe::Result::Status::Applied)
        {
            const auto restored = selectSceneTab(engine, session, closeReturnTab);
            exitDialog.setError(ctx, restored.ok() ? result.error : result.error + " " + restored.error);
            return;
        }
    }

    const auto result = choice == DialogChoice::Cancel
                            ? selectSceneTab(engine, session, closeReturnTab)
                            : closeSceneTab(ctx, engine, session, index, closeReturnTab);
    if (!result.ok() && choice != DialogChoice::Cancel)
    {
        exitDialog.setError(ctx, result.error);
        return;
    }

    exitDialog.hide(ctx);
    closingTab.reset();
    contentBrowserUI.reportError(ctx, result.error);
}

void EditorShell::updateSceneTabs(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto request = contentBrowserUI.takeSceneOpenRequest();
    const bool canSwitch = !engine.isPlaying() && !session.transformEditActive() && !assetDrag.asset.isValid() && !assetDrag.ownsInput && !ctx.contextMenuOwnsInput();
    const auto previousActive = activeTab;
    if (canSwitch && request.isValid())
    {
        const auto *record = engine.getAssetManager().getRegistry().findById(request);
        const auto *project = engine.getProject();
        if (!project || !record || record->type != Assets::AssetType::Scene || record->missing)
        {
            contentBrowserUI.reportError(ctx, "Scene file is unavailable.");
        }
        else
        {
            const std::string path = record->path;
            const auto result = openSceneTab(ctx, engine, session, path);
            contentBrowserUI.reportError(ctx, result.error);
        }
    }
    else if (canSwitch)
    {
        for (std::size_t i = 0; i < sceneTabs.size(); ++i)
        {
            if (ctx.isClicked(sceneTabs[i].close))
            {
                requestSceneTabClose(ctx, engine, session, i);
                break;
            }

            if (!ctx.isClicked(sceneTabs[i].button))
                continue;

            const auto result = selectSceneTab(engine, session, i);
            contentBrowserUI.reportError(ctx, result.error);
            break;
        }
    }

    const float width = ctx.tree().rect(tabBar).w;
    const auto tabCount = sceneTabs.size();

    // Tabs size to their titles, so count how many fit by adding up widths.
    // fitFrom returns how many tabs fit from `first` onwards (at least one).
    const auto fitFrom = [&](std::size_t first, float space) {
        std::size_t count = 0;
        float used = 0.0f;
        for (std::size_t i = first; i < tabCount; ++i)
        {
            const float next = used + (count > 0 ? TabGap : 0.0f) + sceneTabs[i].width;
            if (count > 0 && next > space)
                break;
            used = next;
            ++count;
        }
        return std::max<std::size_t>(count, 1);
    };

    const bool overflow = tabCount > 0 && fitFrom(0, width) < tabCount;
    const float space = width - (overflow ? 2.0f * (ArrowWidth + TabGap) : 0.0f);

    std::size_t lastStart = tabCount;
    {
        std::size_t count = 0;
        float used = 0.0f;
        while (lastStart > 0)
        {
            const float next = used + (count > 0 ? TabGap : 0.0f) + sceneTabs[lastStart - 1].width;
            if (count > 0 && next > space)
                break;
            used = next;
            ++count;
            --lastStart;
        }
    }

    firstVisibleTab = std::min(firstVisibleTab, lastStart);
    if ((previousActive != activeTab || restoreTabSelection) &&
        (activeTab < firstVisibleTab || activeTab >= firstVisibleTab + fitFrom(firstVisibleTab, space)))
        firstVisibleTab = std::min(activeTab, lastStart);

    if (ctx.isClicked(previousTabs) && firstVisibleTab > 0)
        --firstVisibleTab;
    if (ctx.isClicked(nextTabs) && firstVisibleTab < lastStart)
        ++firstVisibleTab;

    const auto visible = fitFrom(firstVisibleTab, space);

    ctx.setVisible(previousTabs, overflow);
    ctx.setVisible(nextTabs, overflow);
    ctx.setEnabled(previousTabs, firstVisibleTab > 0);
    ctx.setEnabled(nextTabs, firstVisibleTab < lastStart);

    const auto *scene = engine.getActiveScene();
    for (std::size_t i = 0; i < tabCount; ++i)
    {
        auto &tab = sceneTabs[i];
        ctx.setVisible(tab.root, i >= firstVisibleTab && i < firstVisibleTab + visible);
        ctx.setEnabled(tab.button, canSwitch);
        ctx.setEnabled(tab.close, canSwitch);
       
        const bool active = i == activeTab;
        const bool hovered = canSwitch && (ctx.isHovered(tab.button) || ctx.isHovered(tab.close));
        Color background = Color::transparent();
        if (active)
            background = hovered ? ActiveTabHovered : ctx.theme().surfaceRaised;
        else if (canSwitch && ctx.isPressed(tab.button))
            background = ctx.theme().surfaceRaised;
        else if (hovered)
            background = ctx.theme().surfaceRaised.withAlpha(0.6f);
        ctx.style(tab.root).all.background = background;
        AccentEdgeStyle edge;

        if (active)
        {
            edge.color = ctx.theme().focus;
            edge.width = 2.0f;
            edge.side = AccentEdgeSide::Top;
        }
        ctx.style(tab.root).accentEdge = edge;
        ctx.style(tab.button).normal.text = active ? ctx.theme().text : ctx.theme().textMuted;

        const bool showDot = tab.dirty && !ctx.isHovered(tab.close);
        ctx.setVisible(tab.closeIcon, !showDot);
        ctx.setIconColor(tab.closeIcon, canSwitch && ctx.isHovered(tab.close) ? ctx.theme().text : ctx.theme().textMuted);

        if (ctx.text(tab.close) != (showDot ? DirtyDot : std::string_view{}))
            ctx.setText(tab.close, showDot ? DirtyDot : std::string_view{});

        if (!tab.document && (!scene || engine.isPlaying()))
            continue;

        const auto &name = tab.document ? tab.document->snapshot.name : scene->getName();
        const auto &path = tab.document ? tab.document->snapshot.resourcePath : scene->getResourcePath();
        const bool dirty = tab.document ? tab.document->snapshot.dirty : scene->needsSave();
        const std::string_view label = name.empty() ? "Untitled" : std::string_view{name};
        tab.dirty = dirty;
        if (ctx.text(tab.button) != label)
        {
            ctx.setText(tab.button, label);

            // Fit the tab to its title: text, title padding, OctoGUI's 8 px text
            // allowance and the close slot, clamped to a sensible range.
            tab.width = std::clamp(std::ceil(ctx.font().measureText(label).x) + SpaceM + SpaceXs + 8.0f + ArrowWidth,
                                   TabMinWidth, TabMaxWidth);
            ctx.layoutStyle(tab.root).preferredSize = Vec2{tab.width, TabHeight};
            auto title = ctx.layoutStyle(tab.button);
            title.minSize = Vec2{tab.width - ArrowWidth, TabHeight};
            title.maxSize = Vec2{tab.width - ArrowWidth, TabHeight};
        }
        ctx.setTooltip(tab.button, path ? std::string_view{*path} : label);
    }
}

Scribe::Result EditorShell::saveSceneTabs(
    Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto returnTab = activeTab;
    Scribe::Result result = Scribe::Result::applied();

    for (std::size_t i = 0; i < sceneTabs.size(); ++i)
    {
        const auto *document = sceneTabs[i].document;
        const bool dirty = document
            ? document->snapshot.dirty
            : engine.unsavedEditSceneName().has_value();

        if (!dirty)
            continue;

        result = selectSceneTab(engine, session, i);
        if (!result.ok())
            break;

        result = saveScene(ctx, engine, session);
        if (result.status != Scribe::Result::Status::Applied)
            break;
    }

    if (!sceneTabs.empty())
    {
        const auto restored =
            selectSceneTab(engine, session, returnTab);

        if (!restored.ok())
        {
            return Scribe::Result::failed(
                result.error.empty()
                    ? restored.error
                    : result.error + " " + restored.error);
        }
    }

    return result;
}
} // namespace Cthulhu::Editor
