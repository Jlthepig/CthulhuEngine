#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cassert>
#include <algorithm>

#include "editorShell.hpp"
#include "engine.hpp"
#include "input.hpp"
#include "pathEncoding.hpp"
#include "platform.hpp"
#include "session.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
namespace
{
bool samePath(const std::filesystem::path &a, const std::filesystem::path &b)
{
#ifdef _WIN32
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return a == b;
#endif
}

std::optional<std::string> sceneResourcePath(const Project::Project &project, const std::filesystem::path &selected)
{
    try
    {
        const auto file = selected.lexically_normal();
        const auto root = project.getRootPath().lexically_normal();
        if (!file.is_absolute())
            return std::nullopt;

        auto part = file.begin();
        for (const auto &rootPart : root)
        {
            if (part == file.end() || !samePath(rootPart, *part))
                return std::nullopt;
            ++part;
        }

        std::filesystem::path relative;
        for (; part != file.end(); ++part)
            relative /= *part;
        if (relative.empty())
            return std::nullopt;

        const auto key = Assets::normaliseResourcePath(
            "res://" + Utils::pathToUtf8(relative));
        if (!key || Assets::getAssetType(*key) != Assets::AssetType::Scene)
            return std::nullopt;

        const auto resolved = project.resolveResourcePath(*key);
        std::error_code error;
        if (!resolved || !std::filesystem::equivalent(file.parent_path(), resolved->parent_path(), error) || error)
        {
            return std::nullopt;
        }

        const bool exists = std::filesystem::exists(file, error);
        if (error || (exists && (!std::filesystem::equivalent(file, *resolved, error) || error)))
        {
            return std::nullopt;
        }

        return key;
    }
    catch (const std::filesystem::filesystem_error &)
    {
        return std::nullopt;
    }
}
} // namespace

Core::Platform::FileDialogResult EditorShell::pickSceneFile(
    Context &ctx, Engine &engine, Scribe::Session &session, bool saving)
{
    using namespace Core::Platform;

    const auto *project = engine.getProject();
    const auto *window = engine.getWindow();
    const auto *scene = engine.getActiveScene();
    if (!project || !window || !window->getWindow() || !scene || engine.isPlaying() || session.transformEditActive())
    {
        return {FileDialogStatus::Failed, {}, "Scene files require Edit mode."};
    }

    const auto projectFile = project->getProjectFilePath();
    const auto generation = engine.getSceneGeneration();
    auto initial = project->getRootPath();

    if (saving)
    {
        initial /= "Untitled.scene";
        if (scene->getResourcePath())
        {
            const auto current = project->resolveResourcePath(*scene->getResourcePath());
            std::error_code error;
            if (current && std::filesystem::is_directory(current->parent_path(), error) && !error)
            {
                initial = *current;
            }
        }
    }

    constexpr FileDialogFilter filters[] = {{"Scenes (*.scene)", "*.scene"}};

    const auto focus = ctx.focused();
    ctx.clearFocus();
    session.endMerge();

    auto picked = saving ? saveFileDialog(*window, filters, "scene", initial) : openFileDialog(*window, filters, initial);

    engine.resetFrameTiming();
    Core::Input::resetMouseTracking();
    ctx.clearEvents();
    uiEvents.clear();
    ctx.input().events.clear();

    if (ctx.isValid(focus) && ctx.isVisible(focus) && ctx.isEnabled(focus))
        ctx.setFocused(focus);

    assert((picked.status == FileDialogStatus::Selected) == !picked.file.empty());

    if (picked.status != FileDialogStatus::Selected)
        return picked;

    project = engine.getProject();
    if (!project || project->getProjectFilePath() != projectFile || engine.getSceneGeneration() != generation || engine.isPlaying() || session.transformEditActive())
    {
        return {
            FileDialogStatus::Failed, {},
            "The project or scene changed. Try again."
        };
    }

    return picked;
}

std::optional<std::size_t> EditorShell::findSceneTab(Engine &engine, std::string_view path, std::optional<std::size_t> skip) const
{
    const auto *project = engine.getProject();
    const auto *scene = engine.getActiveScene();
    if (!project)
        return std::nullopt;

    const auto target = project->resolveResourcePath(path);
    for (std::size_t i = 0; i < sceneTabs.size(); ++i)
    {
        if (skip && i == *skip)
            continue;

        const auto *current = sceneTabs[i].document
            ? &sceneTabs[i].document->snapshot.resourcePath
            : scene ? &scene->getResourcePath() : nullptr;

        if (!current || !*current)
            continue;
        if (**current == path)
            return i;

        const auto file = project->resolveResourcePath(**current);
        std::error_code error;
        if (target && file && (samePath(*target, *file) || (std::filesystem::equivalent(*target, *file, error) && !error)))
        {
            return i;
        }
    }

    return std::nullopt;
}

void EditorShell::rememberSceneFile(Engine &engine, std::string_view path)
{
    const auto *project = engine.getProject();
    if (!project || path.empty())
        return;

    if (recentProject != project->getProjectFilePath())
    {
        recentProject = project->getProjectFilePath();
        recentScenes.clear();
    }

    const std::string savedPath(path);
    const auto target = project->resolveResourcePath(savedPath);
    if (!target)
        return;

    std::erase_if(recentScenes, [&](const std::string &previous) {
        if (previous == savedPath)
            return true;

        const auto file = project->resolveResourcePath(previous);
        std::error_code error;
        return file && (samePath(*target, *file) || (std::filesystem::equivalent(*target, *file, error) && !error));
    });

    recentScenes.insert(recentScenes.begin(), savedPath);
    if (recentScenes.size() > 10)
        recentScenes.resize(10);

    assert(recentScenes.front() == savedPath && recentScenes.size() <= 10);
}

Scribe::Result EditorShell::openSceneTab(Context &ctx, Engine &engine, Scribe::Session &session, std::string_view path)
{
    if (engine.isPlaying() || session.transformEditActive())
        return Scribe::Result::failed("Scene files require Edit mode.");

    if (const auto existing = findSceneTab(engine, path))
    {
        const auto result = selectSceneTab(engine, session, *existing);
        if (result.ok())
            rememberSceneFile(engine, path);
        return result;
    }

    rememberSceneView(engine);
    auto opened = session.openSceneDocument(path);
    if (!opened.result.ok())
        return opened.result;

    if (opened.previous)
    {
        assert(!sceneTabs.empty());
        sceneTabs[activeTab].document = opened.previous;
    }

    activeTab = sceneTabs.size();
    addSceneTab(ctx);
    firstVisibleTab = activeTab;
    restoreTabSelection = true;

    rememberSceneFile(engine, path);
    return opened.result;
}

void EditorShell::openSceneFile(Context &ctx, Engine &engine, Scribe::Session &session)
{
    using Core::Platform::FileDialogStatus;

    const auto picked = pickSceneFile(ctx, engine, session, false);
    if (picked.status == FileDialogStatus::Cancelled)
        return;

    if (picked.status == FileDialogStatus::Failed)
    {
        contentBrowserUI.reportError(ctx, picked.error);
        return;
    }

    const auto path = sceneResourcePath(*engine.getProject(), picked.file);
    if (!path)
    {
        contentBrowserUI.reportError(ctx, "Choose a .scene file inside this project, without links.");
        return;
    }

    const auto result = openSceneTab(ctx, engine, session, *path);
    contentBrowserUI.reportError(ctx, result.error);
}

Scribe::Result EditorShell::saveScene(Context &ctx, Engine &engine, Scribe::Session &session, bool saveAs)
{
    using Core::Platform::FileDialogStatus;

    const auto *scene = engine.getActiveScene();
    if (!scene || engine.isPlaying() || session.transformEditActive())
        return Scribe::Result::failed("There is no editable scene to save.");

    const auto original = scene->getResourcePath();
    std::string path;
    const bool needsPicker = saveAs || !original;

    if (needsPicker)
    {
        const auto picked = pickSceneFile(ctx, engine, session, true);
        if (picked.status == FileDialogStatus::Cancelled)
            return Scribe::Result::noChange();
        if (picked.status == FileDialogStatus::Failed)
            return Scribe::Result::failed(picked.error);

        const auto converted = sceneResourcePath(*engine.getProject(), picked.file);
        if (!converted)
        {
            return Scribe::Result::failed("Choose a .scene filename inside this project, without links.");
        }

        path = *converted;
    }
    else
    {
        path = *original;
    }

    if (findSceneTab(engine, path, activeTab))
    {
        return Scribe::Result::failed("That file belongs to another open scene. Choose a different filename.");
    }

    const auto saved = session.saveAs(path);
    if (!saved.ok())
        return saved;

    rememberSceneFile(engine, path);

    if (needsPicker)
    {
        const auto refreshed = session.refreshAssets();
        if (!refreshed.ok())
        {
            return Scribe::Result::failed("The scene was saved, but the Content Browser could not refresh. " + refreshed.error);
        }
    }

    return saved;
}
} // namespace Cthulhu::Editor