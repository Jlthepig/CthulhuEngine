#include <charconv>
#include <string>
#include <algorithm>

#include "GLFW/glfw3.h"

#include "editorShell.hpp"
#include "editorMetrics.hpp"
#include "editorScenePath.hpp"
#include "engine.hpp"
#include "input.hpp"
#include "project.hpp"
#include "platform.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{

void EditorShell::shutdown(Context &ctx)
{
    projectSettingsWindow.close();
    saveLayout(ctx, true);
}

bool ProjectSettingsWindow::open(const Core::Window &parent, const std::filesystem::path &resources, const Project::ProjectConfig &config)
{
    if (window)
    {
        window->focus();
        return true;
    }

    Core::WindowConfig windowConfig;
    windowConfig.resolution = {900.0f, 600.0f};
    windowConfig.minimumSize = {560, 320};
    windowConfig.visible = false;

    GLFWwindow *previous = glfwGetCurrentContext();
    auto *created = Core::Window::createWindow(windowConfig, "Project Settings", &parent);
    if (!created)
        return false;

    ui.emplace();
    if (!ui->init(created->getWindow(), resources))
    {
        ui.reset();
        glfwMakeContextCurrent(previous);
        Core::Window::destroyWindow(created);
        return false;
    }

    window = created;
    build(config);
    glfwMakeContextCurrent(previous);
    window->show();
    return true;
}

bool ProjectSettingsWindow::deferQuit()
{
    if (!window || !inputEditor.dirty(ui->context()))
        return false;

    quitDeferred = true;
    closePending = !closePrompt;
    window->focus();
    return true;
}

void ProjectSettingsWindow::close()
{
    if (!window)
        return;

    GLFWwindow *native = window->getWindow();
    GLFWwindow *previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(native);
    inputEditor.cancelCapture();
    closePending = closePrompt = quitDeferred = false;
    ui->shutdown();
    ui.reset();
    glfwMakeContextCurrent(previous == native ? nullptr : previous);
    Core::Window::destroyWindow(window);
    window = nullptr;
}

void ProjectSettingsWindow::frame(Engine &engine, Scribe::Session &session)
{
    if (!window)
        return;

    GLFWwindow *native = window->getWindow();
    if (window->shouldClose())
    {
        glfwSetWindowShouldClose(native, GLFW_FALSE);
        closePending = true;
        window->focus();
    }

    int width = 0, height = 0;
    glfwGetFramebufferSize(native, &width, &height);
    if (glfwGetWindowAttrib(native, GLFW_ICONIFIED) || width <= 0 || height <= 0)
    {
        if (inputEditor.capturing())
            inputEditor.finishCapture(ui->context(), false);
        return;
    }

    GLFWwindow *previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(native);
    if (glfwGetCurrentContext() != native)
    {
        glfwMakeContextCurrent(previous);
        return;
    }

    const auto *project = engine.getProject();
    const bool canEdit = project && engine.getWorldMode() == WorldMode::Edit &&
                         !session.transformEditActive();
    const bool focused = glfwGetWindowAttrib(native, GLFW_FOCUSED) == GLFW_TRUE;
    const bool listening = inputEditor.capturing();
    auto &ctx = ui->context();
    inputEditor.setEditable(ctx, canEdit);
    (void)ui->beginFrame(engine.getDeltaTime(), !focused || listening);
    if (listening)
        inputEditor.finishCapture(ctx, focused && canEdit && !closePending);

    bool finishClose = false;
    if (closePending)
    {
        closePending = false;
        if (inputEditor.dirty(ctx))
        {
            closePrompt = true;
            inputDialog.show(ctx, "Apply input changes before closing?",
                             "Your input changes have not been applied.",
                             "Apply & Close", "Discard & Close", "Keep Editing");
        }
        else
            finishClose = true;
    }
    else if (inputDialog.isOpen(ctx))
    {
        if (closePrompt)
            inputDialog.setPrimaryEnabled(ctx, canEdit && inputEditor.renameValid());

        const auto choice = inputDialog.pollChoice(ctx);
        if (choice == DialogChoice::Cancel)
        {
            inputDialog.hide(ctx);
            if (closePrompt)
                quitDeferred = false;
            closePrompt = false;
        }
        else if (closePrompt && choice == DialogChoice::Primary)
        {
            const auto error = inputEditor.apply(ctx, session);
            finishClose = error.empty();
            if (!finishClose)
                inputDialog.setError(ctx, error);
        }
        else if (closePrompt && choice == DialogChoice::Secondary)
            finishClose = true;
        else if (canEdit && choice == DialogChoice::Primary)
        {
            inputDialog.hide(ctx);
            inputEditor.deleteSelected(ctx);
        }
    }
    else if (!listening)
        updateControls(ctx, native, engine, session, canEdit);

    if (finishClose)
    {
        const bool quit = quitDeferred;
        ctx.endFrame();
        glfwMakeContextCurrent(previous);
        close();
        if (quit)
            glfwSetWindowShouldClose(engine.getWindow()->getWindow(), GLFW_TRUE);
        return;
    }

    ctx.setText(tabs[inputPage], inputEditor.dirty(ctx) ? "Input *" : "Input");
    const auto background = ctx.theme().background;
    glClearColor(background.r, background.g, background.b, background.a);
    glClear(GL_COLOR_BUFFER_BIT);
    ctx.resolvePendingLayout();
    ui->endFrame();
    glfwSwapBuffers(native);
    glfwMakeContextCurrent(previous);
}

void ProjectSettingsWindow::updateControls(Context &ctx, GLFWwindow *native, Engine &engine, Scribe::Session &session, bool canEdit)
{
    const auto *project = engine.getProject();
    const bool onInput = ctx.isVisible(pages[inputPage]);
    for (const auto control : {projectName, windowWidth, windowHeight, chooseMainScene})
        ctx.setEnabled(control, canEdit);
    ctx.setEnabled(clearMainScene, canEdit && project->hasMainScene());
    ctx.setEnabled(applyButton, canEdit &&
        (!onInput || (inputEditor.dirty(ctx) && inputEditor.renameValid())));

    focusOrder.assign(tabs.begin(), tabs.end());
    focusOrder.insert(focusOrder.end(), {projectName, chooseMainScene, clearMainScene, windowWidth, windowHeight});
    if (onInput)
        inputEditor.appendControls(focusOrder);
    focusOrder.insert(focusOrder.end(), {applyButton, closeButton});
    std::erase_if(focusOrder, [&](NodeHandle node) {
        return !ctx.isEnabled(node) || !ctx.tree().isVisibleBranch(node);
    });

    NodeHandle activated{}, entered{};
    for (const auto &event : ctx.input().events)
    {
        if (event.type != InputEventType::Key || event.action != KeyAction::Press)
            continue;

        const auto focus = ctx.focused();
        const auto *combo = ctx.tree().comboBoxState(focus);
        if (event.key == Key::Escape)
        {
            if (combo && combo->open)
            {
                ctx.clearFocus();
                ctx.setFocused(focus);
            }
            else if (!onInput || !inputEditor.cancelNameEdit(ctx))
                closePending = true;
        }
        else if (event.key == Key::Tab && !focusOrder.empty())
        {
            auto current = std::ranges::find(focusOrder, focus);
            if (event.modifiers.shift)
                current = current == focusOrder.begin() ? focusOrder.end() - 1 : current - 1;
            else if (current == focusOrder.end() || ++current == focusOrder.end())
                current = focusOrder.begin();

            ctx.setFocused(*current);
            for (const auto page : pages)
            {
                if (!ctx.isVisible(page))
                    continue;
                const Rect rect = ctx.tree().rect(*current);
                const Rect area = ctx.tree().rect(page);
                const float offset = ctx.scrollOffsetY(page);
                if (ctx.tree().isDescendant(page, *current))
                {
                    if (rect.y - offset < area.y)
                        ctx.setScrollOffsetY(page, rect.y - area.y);
                    else if (rect.maxY() - offset > area.maxY())
                        ctx.setScrollOffsetY(page, rect.maxY() - area.maxY());
                }
                break;
            }
        }
        else if (combo && ctx.isEnabled(focus) && !combo->items.empty() && (event.key == Key::Up || event.key == Key::Down))
        {
            const auto last = static_cast<i32>(combo->items.size() - 1);
            const auto index = ctx.comboBoxSelectedIndex(focus);
            ctx.setComboBoxSelectedIndex(focus, std::clamp(index + (event.key == Key::Up ? -1 : 1), 0, last));
        }
        else if (event.key == Key::Enter || event.key == Key::Space)
        {
            activated = focus;
            if (event.key == Key::Enter)
                entered = focus;
        }
    }

    const auto chosen = [&](NodeHandle node) {
        return ctx.isEnabled(node) && ctx.tree().isVisibleBranch(node) &&
               (ctx.isClicked(node) || activated == node);
    };

    if (chosen(closeButton) || closePending)
    {
        closePending = true;
        return;
    }

    if (onInput)
        inputEditor.update(ctx, native, inputDialog, activated, entered, canEdit);
    if (inputDialog.isOpen(ctx) || inputEditor.capturing())
        return;

    if (chosen(applyButton))
    {
        if (onInput)
        {
            const auto error = inputEditor.apply(ctx, session);
            if (!error.empty())
                inputEditor.showNotice(ctx, error);
        }
        else
            applyDetails(session);
    }

    for (std::size_t i = 0; i < tabs.size(); ++i)
        if (chosen(tabs[i]))
            selectPage(i);

    if (canEdit && chosen(chooseMainScene))
    {
        const auto result = pickProjectMainScene(ctx, *window, engine, session);
        if (result.status != Scribe::Result::Status::NoChange)
            showMainSceneMessage(result.error);
    }
    else if (canEdit && chosen(clearMainScene))
        showMainSceneMessage(session.setMainScene({}).error);

    if (project && ctx.isVisible(pages[1]))
    {
        const auto *path = project->getMainScene(engine.getAssetManager().getRegistry());
        const std::string_view value = path ? std::string_view(*path) :
            (project->hasMainScene() ? "Unavailable" : "None");
        if (ctx.tree().text(mainSceneValue) != value)
        {
            ctx.setText(mainSceneValue, value);
            ctx.setTooltip(mainSceneValue, value);
        }
    }
}

void ProjectSettingsWindow::build(const Project::ProjectConfig &config)
{
    auto &ctx = ui->context();
    const auto root = ctx.ensureRoot();
    ctx.layoutStyle(root).mode = LayoutMode::Stack;
    ctx.layoutStyle(root).horizontalAlignment = Alignment::Stretch;
    ctx.layoutStyle(root).verticalAlignment = Alignment::Stretch;
    const auto contentRoot = ctx.createContainer(root);
    ctx.layoutStyle(contentRoot).mode = LayoutMode::Vertical;
    ctx.layoutStyle(contentRoot).padding = Padding{SpaceXl};
    ctx.layoutStyle(contentRoot).gap = SpaceL;

    const auto body = ctx.createContainer(contentRoot);
    ctx.layoutStyle(body).mode = LayoutMode::Horizontal;
    ctx.layoutStyle(body).verticalAlignment = Alignment::Stretch;
    ctx.layoutStyle(body).gap = SpaceL;

    const auto sidebar = ctx.createPanel(body);
    ctx.layoutStyle(sidebar).preferredSize = Vec2{164.0f, 0.0f};
    ctx.layoutStyle(sidebar).padding = Padding{SpaceM};
    ctx.layoutStyle(sidebar).gap = SpaceS;

    const auto content = ctx.createContainer(body);
    ctx.layoutStyle(content).mode = LayoutMode::Stack;

    constexpr std::array titles{"General", "Startup", "Window", "Input"};
    for (std::size_t i = 0; i < tabs.size(); ++i)
    {
        tabs[i] = ctx.createButton(sidebar, titles[i]);
        ctx.setTextAlign(tabs[i], TextAlign::Start, TextAlign::Center);
        ctx.layoutStyle(tabs[i]).preferredSize = Vec2{0.0f, PanelHeaderHeight};

        pages[i] = ctx.createPanel(content);
        ctx.layoutStyle(pages[i]).padding = Padding{SpaceXl};
        ctx.layoutStyle(pages[i]).gap = SpaceM;
        ctx.layoutStyle(pages[i]).scrollY = true;

        const auto heading = ctx.createLabel(pages[i], titles[i]);
        ctx.setFontSlot(heading, FontSlot::Secondary);
        ctx.layoutStyle(heading).preferredSize = Vec2{0.0f, PanelHeaderHeight};
        (void)ctx.createSeparator(pages[i]);
    }

    const auto addRow = [&](NodeHandle page, std::string_view label)
    {
        const auto row = ctx.createPanel(page);
        ctx.style(row).all.background = ctx.theme().surfaceRaised;
        auto layout = ctx.layoutStyle(row);
        layout.mode = LayoutMode::Horizontal;
        layout.verticalAlignment = Alignment::Stretch;
        layout.preferredSize = Vec2{0.0f, ControlHeight};
        layout.padding = Padding{SpaceM, 0.0f};
        layout.gap = SpaceM;

        const auto name = ctx.createLabel(row, label);
        ctx.style(name).all.text = ctx.theme().textMuted;
        ctx.layoutStyle(name).preferredSize = Vec2{128.0f, 0.0f};

        return row;
    };

    projectName = ctx.createTextInput(addRow(pages[0], "Project Name"), config.name);
    mainSceneValue = ctx.createLabel(addRow(pages[1], "Main Scene"), "None");
    ctx.layoutStyle(mainSceneValue).preferredSize = Vec2{};
    ctx.layoutStyle(mainSceneValue).clip = true;

    const auto actions = ctx.createContainer(pages[1]);
    auto actionLayout = ctx.layoutStyle(actions);
    actionLayout.mode = LayoutMode::Horizontal;
    actionLayout.verticalAlignment = Alignment::Stretch;
    actionLayout.preferredSize = Vec2{0.0f, DialogButtonHeight};
    actionLayout.gap = SpaceM;

    chooseMainScene = ctx.createButton(actions, "Choose Scene...");
    clearMainScene = ctx.createButton(actions, "Clear");
    ctx.layoutStyle(chooseMainScene).preferredSize = Vec2{148.0f, DialogButtonHeight};
    ctx.layoutStyle(clearMainScene).preferredSize = Vec2{80.0f, DialogButtonHeight};

    mainSceneNotice = ctx.createLabel(pages[1], "");
    ctx.layoutStyle(mainSceneNotice).preferredSize = Vec2{0.0f, RowHeight};
    ctx.layoutStyle(mainSceneNotice).clip = true;
    ctx.setVisible(mainSceneNotice, false);

    windowWidth = ctx.createTextInput(addRow(pages[2], "Width"), std::to_string(config.windowWidth));
    windowHeight = ctx.createTextInput(addRow(pages[2], "Height"), std::to_string(config.windowHeight));

    for (const auto field : {projectName, windowWidth, windowHeight})
    {
        ctx.layoutStyle(field).preferredSize = Vec2{};
        ctx.layoutStyle(field).clip = true;
    }

    inputEditor.build(ctx, pages[inputPage], config.input);

    const auto footer = ctx.createContainer(contentRoot);
    ctx.layoutStyle(footer).mode = LayoutMode::Horizontal;
    ctx.layoutStyle(footer).horizontalAlignment = Alignment::End;
    ctx.layoutStyle(footer).verticalAlignment = Alignment::Stretch;
    ctx.layoutStyle(footer).preferredSize = Vec2{0.0f, DialogButtonHeight};
    ctx.layoutStyle(footer).gap = SpaceM;
    detailsNotice = ctx.createLabel(footer, "");
    ctx.layoutStyle(detailsNotice).preferredSize = Vec2{};
    ctx.layoutStyle(detailsNotice).clip = true;

    applyButton = ctx.createButton(footer, "Apply");
    ctx.layoutStyle(applyButton).preferredSize = Vec2{92.0f, DialogButtonHeight};

    closeButton = ctx.createButton(footer, "Close");
    ctx.layoutStyle(closeButton).preferredSize = Vec2{92.0f, DialogButtonHeight};

    inputDialog.build(ctx, root, contentRoot, 500.0f);
    selectPage(0);
}

void ProjectSettingsWindow::applyDetails(Scribe::Session &session)
{
    auto &ctx = ui->context();
    const auto dimension = [&](NodeHandle field) -> uint32_t {
        const auto value = ctx.text(field);
        if (value.empty())
            return 0;

        uint32_t number = 0;
        const auto *end = value.data() + value.size();
        const auto parsed = std::from_chars(value.data(), end, number);
        return parsed.ec == std::errc{} && parsed.ptr == end ? number : 0;
    };

    const auto result = session.setProjectDetails(ctx.text(projectName),
                                                  dimension(windowWidth), dimension(windowHeight));
    const std::string_view message = result.ok()
        ? "Saved. Reopen the project to apply."
        : std::string_view(result.error);
    ctx.setText(detailsNotice, message);
    ctx.setTooltip(detailsNotice, message);
}

Scribe::Result pickProjectMainScene(Context &ctx, const Core::Window &owner, Engine &engine, Scribe::Session &session)
{
    const auto *project = engine.getProject();
    if (!project)
        return Scribe::Result::failed("No open project.");

    using namespace Core::Platform;
    constexpr FileDialogFilter filters[] = {{"Scenes (*.scene)", "*.scene"}};
    const auto picked = openFileDialog(owner, filters, project->getRootPath());
    engine.resetFrameTiming();
    Core::Input::resetMouseTracking();
    ctx.clearEvents();
    ctx.input().events.clear();

    if (picked.status == FileDialogStatus::Cancelled)
        return Scribe::Result::noChange();
    if (picked.status == FileDialogStatus::Failed)
        return Scribe::Result::failed(picked.error);

    const auto selected = sceneResourcePath(*project, picked.file);
    if (!selected)
        return Scribe::Result::failed("Choose a saved .scene file inside this project.");

    auto result = session.setMainScene(*selected);
    if (result.ok())
        result.status = Scribe::Result::Status::Applied;

    return result;
}

void ProjectSettingsWindow::showMainSceneMessage(std::string_view message)
{
    auto &ctx = ui->context();
    ctx.setText(mainSceneNotice, message);
    ctx.setTooltip(mainSceneNotice, message);
    ctx.setVisible(mainSceneNotice, !message.empty());
}

void ProjectSettingsWindow::selectPage(std::size_t index)
{
    auto &ctx = ui->context();
    for (std::size_t i = 0; i < pages.size(); ++i)
    {
        ctx.setVisible(pages[i], i == index);
        ctx.style(tabs[i]).normal.background =
            i == index ? ctx.theme().surfaceActive : ctx.theme().surface;
    }
    ctx.setVisible(detailsNotice, index != inputPage);
    ctx.setFocused(tabs[index]);
}

bool EditorShell::updateProjectMenu(Context &ctx, Engine &engine, Scribe::Session &session)
{
    constexpr u64 OpenSettings = 1;
    const auto *project = engine.getProject();
    const bool canOpenSettings = project && engine.getWorldMode() == WorldMode::Edit && !session.transformEditActive() && !assetDrag.asset.isValid() &&
        !assetDrag.ownsInput && !contentBrowserUI.actionPending() && !restoreTabSelection;

    ContextMenuSelection choice;
    if (ctx.pollContextMenuSelection(choice, projectButton))
    {
        if (canOpenSettings && choice.action == OpenSettings && !projectSettingsWindow.open(*engine.getWindow(),
            engine.getEngineResourceRoot(), project->getConfig()))
        {
            contentBrowserUI.reportError(ctx, "Could not open Project Settings. See the log.");
        }
        return true;
    }

    if (!ctx.isClicked(projectButton))
        return false;

    const ContextMenuItem items[] = {
        {OpenSettings, "Project Settings", canOpenSettings},
        {0, "Export (unavailable)", false}
    };

    const Rect button = ctx.visibleRect(projectButton);
    if (ctx.openContextMenu(projectButton, {button.x, button.maxY()}, items))
        session.endMerge();
    else
        contentBrowserUI.reportError(ctx, "Could not open the Project menu.");

    return true;
}
} // namespace Cthulhu::Editor