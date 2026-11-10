#include "contentBrowserPanel.hpp"

#include <cassert>
#include <filesystem>
#include <utility>

#include "engine.hpp"
#include "pathEncoding.hpp"
#include "platform.hpp"
#include "project.hpp"
#include "session.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
void ContentBrowserPanel::importAsset(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto showError = [&](std::string message) {
        actionError = std::move(message);
        ctx.setText(errorLabel, actionError);
        ctx.setTooltip(errorLabel, actionError);
        ctx.setVisible(errorLabel, !actionError.empty());
    };

    const auto *project = engine.getProject();
    const auto folderIndex = index.findFolder(currentFolder);
    if (!project || !engine.getWindow() || engine.isPlaying() || session.transformEditActive() || !folderIndex || index.entries()[*folderIndex].missing)
        return;

    const std::filesystem::path projectFile = project->getProjectFilePath();
    const std::string folder = currentFolder;

    constexpr Core::Platform::FileDialogFilter filters[] = {
        {"Supported assets", "*.glb;*.wav;*.mp3;*.flac;*.scene"},
        {"Models (*.glb)", "*.glb"},
        {"Audio (*.wav;*.mp3;*.flac)", "*.wav;*.mp3;*.flac"},
        {"Scenes (*.scene)", "*.scene"}
    };

    ctx.clearFocus();
    auto picked = Core::Platform::openFileDialog(*engine.getWindow(), filters);
    ctx.setFocused(importButton);

    assert((picked.status == Core::Platform::FileDialogStatus::Selected) == !picked.file.empty());

    if (picked.status == Core::Platform::FileDialogStatus::Cancelled) return;
    if (picked.status == Core::Platform::FileDialogStatus::Failed)
    {
        showError(std::move(picked.error));
        return;
    }

    project = engine.getProject();
    if (!project || project->getProjectFilePath() != projectFile || engine.isPlaying() || session.transformEditActive())
    {
        showError("The project or editing mode changed. Try importing again.");
        return;
    }

    std::error_code error;
    const auto directory = project->resolveResourcePath(folder);
    if (!directory || !std::filesystem::is_directory(*directory, error) || error)
    {
        showError("The destination folder is unavailable. Refresh and try again.");
        return;
    }

    try
    {
        const std::string destination = folder + Utils::pathToUtf8(picked.file.filename());
        const auto file = project->resolveResourcePath(destination);

        if (!file || Assets::getAssetType(destination) == Assets::AssetType::Unknown)
        {
            showError("Choose a supported model, audio, or scene file.");
            return;
        }

        if (engine.getAssetManager().getRegistry().findByPath(destination))
        {
            showError("An asset with this name is already registered in this folder.");
            return;
        }

        const bool exists = std::filesystem::exists(*file, error);
        if (error)
        {
            showError("Could not check the destination: " + error.message());
            return;
        }
        if (exists)
        {
            showError("A file with this name already exists in this folder.");
            return;
        }

        const auto result = session.importAsset(picked.file, destination);
        if (!result.ok())
        {
            showError(result.result.error);
            return;
        }

        ctx.setText(searchInput, "");
        searchText.clear();
        appliedType = 0;
        ctx.setComboBoxSelectedIndex(typeFilter, 0);
        showFolder(ctx, folder);
        selectedAsset = result.id;
        showError("");
    }
    catch (const std::filesystem::filesystem_error &)
    {
        showError("This filename cannot be represented as a UTF-8 resource path.");
    }
}
} // namespace Cthulhu::Editor