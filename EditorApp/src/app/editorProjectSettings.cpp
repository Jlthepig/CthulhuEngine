#include <charconv>
#include <string>
#include <algorithm>

#include "GLFW/glfw3.h"

#include "editorShell.hpp"
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

void ProjectSettingsWindow::close()
{
    if (!window)
        return;

    GLFWwindow *native = window->getWindow();
    GLFWwindow *previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(native);
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

    if (window->shouldClose())
    {
        close();
        return;
    }

    GLFWwindow *native = window->getWindow();
    int width = 0, height = 0;
    glfwGetFramebufferSize(native, &width, &height);
    if (glfwGetWindowAttrib(native, GLFW_ICONIFIED) || width <= 0 || height <= 0)
        return;

    GLFWwindow *previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(native);
    if (glfwGetCurrentContext() != native)
    {
        glfwMakeContextCurrent(previous);
        return;
    }

    const bool focused = glfwGetWindowAttrib(native, GLFW_FOCUSED) == GLFW_TRUE;
    (void)ui->beginFrame(engine.getDeltaTime(), !focused);
    auto &ctx = ui->context();

    const auto *project = engine.getProject();
    const bool canEdit = project && engine.getWorldMode() == WorldMode::Edit &&
                         !session.transformEditActive();
    for (const auto control : {projectName, windowWidth, windowHeight, applyButton})
        ctx.setEnabled(control, canEdit);
    ctx.setEnabled(chooseMainScene, canEdit);
    ctx.setEnabled(clearMainScene, canEdit && project->hasMainScene());

    bool dismiss = ctx.isClicked(closeButton);
    NodeHandle activated{};
    const std::array controls{tabs[0], tabs[1], tabs[2], projectName, chooseMainScene, clearMainScene,
                              windowWidth, windowHeight, applyButton, closeButton};
    const auto available = [&](NodeHandle control) {
        if (!ctx.isEnabled(control))
            return false;
        if (control == projectName)
            return ctx.isVisible(pages[0]);
        if (control == chooseMainScene || control == clearMainScene)
            return ctx.isVisible(pages[1]);
        if (control == windowWidth || control == windowHeight)
            return ctx.isVisible(pages[2]);
        return true;
    };

    for (const auto &event : ctx.input().events)
    {
        if (event.type != InputEventType::Key || event.action != KeyAction::Press)
            continue;

        if (event.key == Key::Escape)
            dismiss = true;
        else if (event.key == Key::Tab)
        {
            const auto current = std::find(controls.begin(), controls.end(), ctx.focused());
            const auto count = controls.size();
            const auto step = event.modifiers.shift ? count - 1 : 1;
            auto index = current == controls.end()
                ? (event.modifiers.shift ? 0 : count - 1)
                : static_cast<std::size_t>(current - controls.begin());
            do
            {
                index = (index + step) % count;
            } while (!available(controls[index]));
            ctx.setFocused(controls[index]);
        }
        else if (event.key == Key::Enter || event.key == Key::Space)
            activated = ctx.focused();
    }

    dismiss = dismiss || activated == closeButton;
    if (dismiss)
    {
        ctx.endFrame();
        glfwMakeContextCurrent(previous);
        close();
        return;
    }

    for (std::size_t i = 0; i < tabs.size(); ++i)
        if (ctx.isClicked(tabs[i]) || activated == tabs[i])
            selectPage(i);

    if (canEdit && (ctx.isClicked(applyButton) || activated == applyButton))
        applyDetails(session);

    if (canEdit && ctx.isVisible(pages[1]))
    {
        if (ctx.isClicked(chooseMainScene) || activated == chooseMainScene)
            pickMainScene(engine, session);
        else if (ctx.isEnabled(clearMainScene) && (ctx.isClicked(clearMainScene) || activated == clearMainScene))
            showMainSceneMessage(session.setMainScene({}).error);
    }

    if (const auto *currentProject = engine.getProject(); currentProject && ctx.isVisible(pages[1]))
    {
        const auto *path = currentProject->getMainScene(engine.getAssetManager().getRegistry());
        const std::string_view value = path ? std::string_view(*path) :
            (currentProject->hasMainScene() ? "Unavailable" : "None");
        if (ctx.tree().text(mainSceneValue) != value)
        {
            ctx.setText(mainSceneValue, value);
            ctx.setTooltip(mainSceneValue, value);
        }
    }

    const auto background = ctx.theme().background;
    glClearColor(background.r, background.g, background.b, background.a);
    glClear(GL_COLOR_BUFFER_BIT);
    ctx.resolvePendingLayout();
    ui->endFrame();
    glfwSwapBuffers(native);
    glfwMakeContextCurrent(previous);
}

void ProjectSettingsWindow::build(const Project::ProjectConfig &config)
{
    auto &ctx = ui->context();
    const auto root = ctx.ensureRoot();
    ctx.layoutStyle(root).mode = LayoutMode::Vertical;
    ctx.layoutStyle(root).padding = Padding{16.0f};
    ctx.layoutStyle(root).gap = 12.0f;

    const auto body = ctx.createContainer(root);
    ctx.layoutStyle(body).mode = LayoutMode::Horizontal;
    ctx.layoutStyle(body).verticalAlignment = Alignment::Stretch;
    ctx.layoutStyle(body).gap = 12.0f;

    const auto sidebar = ctx.createPanel(body);
    ctx.layoutStyle(sidebar).preferredSize = Vec2{164.0f, 0.0f};
    ctx.layoutStyle(sidebar).padding = Padding{8.0f};
    ctx.layoutStyle(sidebar).gap = 4.0f;

    const auto content = ctx.createContainer(body);
    ctx.layoutStyle(content).mode = LayoutMode::Stack;

    constexpr std::array titles{"General", "Startup", "Window"};
    for (std::size_t i = 0; i < tabs.size(); ++i)
    {
        tabs[i] = ctx.createButton(sidebar, titles[i]);
        ctx.setTextAlign(tabs[i], TextAlign::Start, TextAlign::Center);
        ctx.layoutStyle(tabs[i]).preferredSize = Vec2{0.0f, 34.0f};

        pages[i] = ctx.createPanel(content);
        ctx.layoutStyle(pages[i]).padding = Padding{16.0f};
        ctx.layoutStyle(pages[i]).gap = 8.0f;
        ctx.layoutStyle(pages[i]).scrollY = true;

        const auto heading = ctx.createLabel(pages[i], titles[i]);
        ctx.layoutStyle(heading).preferredSize = Vec2{0.0f, 34.0f};
        (void)ctx.createSeparator(pages[i]);
    }

    const auto addRow = [&](NodeHandle page, std::string_view label)
    {
        const auto row = ctx.createPanel(page);
        ctx.style(row).all.background = ctx.theme().surfaceRaised;
        auto layout = ctx.layoutStyle(row);
        layout.mode = LayoutMode::Horizontal;
        layout.verticalAlignment = Alignment::Stretch;
        layout.preferredSize = Vec2{0.0f, 34.0f};
        layout.padding = Padding{8.0f, 0.0f};
        layout.gap = 8.0f;

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
    actionLayout.preferredSize = Vec2{0.0f, 34.0f};
    actionLayout.gap = 8.0f;

    chooseMainScene = ctx.createButton(actions, "Choose Scene...");
    clearMainScene = ctx.createButton(actions, "Clear");
    ctx.layoutStyle(chooseMainScene).preferredSize = Vec2{148.0f, 34.0f};
    ctx.layoutStyle(clearMainScene).preferredSize = Vec2{80.0f, 34.0f};

    mainSceneNotice = ctx.createLabel(pages[1], "");
    ctx.layoutStyle(mainSceneNotice).preferredSize = Vec2{0.0f, 30.0f};
    ctx.layoutStyle(mainSceneNotice).clip = true;
    ctx.setVisible(mainSceneNotice, false);

    windowWidth = ctx.createTextInput(addRow(pages[2], "Width"), std::to_string(config.windowWidth));
    windowHeight = ctx.createTextInput(addRow(pages[2], "Height"), std::to_string(config.windowHeight));

    for (const auto field : {projectName, windowWidth, windowHeight})
    {
        ctx.layoutStyle(field).preferredSize = Vec2{};
        ctx.layoutStyle(field).clip = true;
    }

    const auto footer = ctx.createContainer(root);
    ctx.layoutStyle(footer).mode = LayoutMode::Horizontal;
    ctx.layoutStyle(footer).horizontalAlignment = Alignment::End;
    ctx.layoutStyle(footer).verticalAlignment = Alignment::Stretch;
    ctx.layoutStyle(footer).preferredSize = Vec2{0.0f, 34.0f};
    ctx.layoutStyle(footer).gap = 8.0f;
    detailsNotice = ctx.createLabel(footer, "");
    ctx.layoutStyle(detailsNotice).preferredSize = Vec2{};
    ctx.layoutStyle(detailsNotice).clip = true;

    applyButton = ctx.createButton(footer, "Apply");
    ctx.layoutStyle(applyButton).preferredSize = Vec2{92.0f, 34.0f};

    closeButton = ctx.createButton(footer, "Close");
    ctx.layoutStyle(closeButton).preferredSize = Vec2{92.0f, 34.0f};

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

void ProjectSettingsWindow::pickMainScene(Engine &engine, Scribe::Session &session)
{
    const auto *project = engine.getProject();
    if (!project)
        return;

    using namespace Core::Platform;
    constexpr FileDialogFilter filters[] = {{"Scenes (*.scene)", "*.scene"}};
    const auto picked = openFileDialog(*window, filters, project->getRootPath());
    engine.resetFrameTiming();
    Core::Input::resetMouseTracking();
    ui->context().clearEvents();
    ui->context().input().events.clear();

    if (picked.status == FileDialogStatus::Cancelled)
        return;
    if (picked.status == FileDialogStatus::Failed)
    {
        showMainSceneMessage(picked.error);
        return;
    }

    const auto path = sceneResourcePath(*project, picked.file);
    if (!path)
    {
        showMainSceneMessage("Choose a saved .scene file inside this project.");
        return;
    }

    showMainSceneMessage(session.setMainScene(*path).error);
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