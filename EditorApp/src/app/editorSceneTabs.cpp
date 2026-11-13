#include "editorShell.hpp"

#include <algorithm>
#include <cassert>

#include "engine.hpp"
#include "input.hpp"
#include "session.hpp"

namespace Cthulhu::Editor
{
namespace
{
constexpr float TabWidth = 164.0f;
constexpr float TabHeight = 28.0f;
constexpr float TabGap = 16.0f;
constexpr float ArrowWidth = 24.0f;
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

NodeHandle tabIconButton(Context &ctx, NodeHandle parent, std::string_view icon, std::string_view tooltip)
{
    const auto button = ctx.createButton(parent,"");
    auto layout = ctx.layoutStyle(button);
    layout.mode = LayoutMode::Horizontal;
    layout.preferredSize = {ArrowWidth, TabHeight};
    layout.horizontalAlignment = Alignment::Center;
    layout.verticalAlignment = Alignment::Center;
    styleTabButton(ctx, button);

    const auto image = ctx.createIcon(button, icon);
    ctx.setIconSize(image, 14.0f);
    ctx.setTooltip(button, tooltip);
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
    layout.preferredSize = {TabWidth, TabHeight};
    layout.verticalAlignment = Alignment::Stretch;
    ctx.style(tab.root).cornerRadius = 0.0f;

    tab.button = ctx.createButton(tab.root, "");
    ctx.setTextAlign(tab.button, TextAlign::Start, TextAlign::Center);
    auto title = ctx.layoutStyle(tab.button);
    title.minSize = {TabWidth - ArrowWidth, TabHeight};
    title.maxSize = {TabWidth - ArrowWidth, TabHeight};
    title.padding = Padding{10.0f, 0.0f, 2.0f, 0.0f};
    title.clip = true;
    styleTabButton(ctx, tab.button);
    tab.close = tabIconButton(ctx, tab.root, "close", "Close scene");
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

void EditorShell::handleSceneTabClose(Context &ctx, Engine &engine, Scribe::Session &session, DialogChoice choice)
{
    if (choice == DialogChoice::None)
        return;

    const auto index = *closingTab;
    if (choice == DialogChoice::Primary)
    {
        auto result = selectSceneTab(engine, session, index);
        if (result.ok())
            result = session.save();
        if (!result.ok())
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
    const bool canSwitch = !engine.isPlaying() && !assetDrag.asset.isValid() && !assetDrag.ownsInput;
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
            const auto file = project->resolveResourcePath(path);
            const auto *scene = engine.getActiveScene();
            const auto matches = [&](const SceneTab &tab) {
                const auto *current = tab.document ? &tab.document->snapshot.resourcePath
                                                    : scene ? &scene->getResourcePath() : nullptr;
                if (!current || !*current)
                    return false;
                if (**current == path)
                    return true;

                const auto openFile = project->resolveResourcePath(**current);
                std::error_code error;
                return file && openFile && std::filesystem::equivalent(*file, *openFile, error) && !error;
            };

            const auto existing = std::find_if(sceneTabs.begin(), sceneTabs.end(), matches);
            if (existing != sceneTabs.end())
            {
                const auto result = selectSceneTab(engine, session, static_cast<std::size_t>(existing - sceneTabs.begin()));
                contentBrowserUI.reportError(ctx, result.error);
            }
            else
            {
                rememberSceneView(engine);
                auto opened = session.openSceneDocument(path);
                if (opened.result.ok())
                {
                    if (opened.previous)
                    {
                        assert(!sceneTabs.empty());
                        sceneTabs[activeTab].document = opened.previous;
                    }

                    activeTab = sceneTabs.size();
                    addSceneTab(ctx);
                    restoreTabSelection = true;
                }
                contentBrowserUI.reportError(ctx, opened.result.error);
            }
        }
    }
    else if (canSwitch)
    {
        for (std::size_t i = 0; i < sceneTabs.size(); ++i)
        {
            if (ctx.isClicked(sceneTabs[i].close))
            {
                auto dirtyName = engine.unsavedEditSceneName();
                if (const auto *doc = sceneTabs[i].document)
                {
                    dirtyName.reset();
                    if (doc->snapshot.dirty)
                        dirtyName = doc->snapshot.name;
                }

                if (dirtyName)
                {
                    closingTab = i;
                    closeReturnTab = activeTab;
                    exitDialog.show(ctx, "Save changes before closing?",
                                    dirtyName->empty() ? "Untitled" : *dirtyName,
                                    "Save", "Discard Changes");
                }
                else
                {
                    const auto result = closeSceneTab(ctx, engine, session, i, activeTab);
                    contentBrowserUI.reportError(ctx, result.error);
                }
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
    const auto capacity = [](float space) {
        return static_cast<std::size_t>(std::max(1.0f, (space + TabGap) / (TabWidth + TabGap)));
    };
    const bool overflow = tabCount > capacity(width);
    const auto visible = capacity(width - (overflow ? 2.0f * (ArrowWidth + TabGap) : 0.0f));
    const auto lastStart = tabCount > visible ? tabCount - visible : 0;
    firstVisibleTab = std::min(firstVisibleTab, lastStart);
    if ((previousActive != activeTab || restoreTabSelection) &&
        (activeTab < firstVisibleTab || activeTab >= firstVisibleTab + visible))
        firstVisibleTab = std::min(activeTab, lastStart);

    if (ctx.isClicked(previousTabs) && firstVisibleTab > 0)
        --firstVisibleTab;
    if (ctx.isClicked(nextTabs) && firstVisibleTab < lastStart)
        ++firstVisibleTab;

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
        ctx.style(tab.root).all.background = i == activeTab ? ctx.theme().surface : Color::transparent();
        ctx.style(tab.button).normal.text = i == activeTab ? ctx.theme().text : ctx.theme().textMuted;

        if (!tab.document && (!scene || engine.isPlaying()))
            continue;

        const auto &name = tab.document ? tab.document->snapshot.name : scene->getName();
        const auto &path = tab.document ? tab.document->snapshot.resourcePath : scene->getResourcePath();
        const bool dirty = tab.document ? tab.document->snapshot.dirty : scene->needsSave();
        const std::string_view label = name.empty() ? "Untitled" : std::string_view{name};
        const auto current = ctx.text(tab.button);
        const bool labelMatches = current.starts_with(label) && current.substr(label.size()) == (dirty ? " *" : "");
        if (!labelMatches)
            ctx.setText(tab.button, dirty ? std::string(label) + " *" : std::string(label));
        ctx.setTooltip(tab.button, path ? std::string_view{*path} : label);
    }
}

Scribe::Result EditorShell::saveSceneTabs(Engine &engine, Scribe::Session &session)
{
    if (engine.unsavedEditSceneName())
    {
        const auto saved = session.save();
        if (!saved.ok())
            return Scribe::Result::failed(engine.getActiveScene()->getName() + ": " + saved.error);
    }

    for (std::size_t i = 0; i < sceneTabs.size(); ++i)
    {
        const auto *document = sceneTabs[i].document;
        if (!document || !document->snapshot.dirty)
            continue;

        const auto switched = selectSceneTab(engine, session, i);
        if (!switched.ok())
            return switched;

        const auto saved = session.save();
        if (!saved.ok())
            return Scribe::Result::failed(engine.getActiveScene()->getName() + ": " + saved.error);
    }

    return Scribe::Result::applied();
}
} // namespace Cthulhu::Editor
