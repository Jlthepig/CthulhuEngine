#include <GLFW/glfw3.h>

#include "editorShell.hpp"

#include <algorithm>
#include <cassert>

#include "components.hpp"
#include "engine.hpp"
#include "input.hpp"
#include "window.hpp"

namespace Cthulhu::Editor
{
namespace
{
std::vector<Scribe::MissingAsset> repairGroupsFor(Engine &engine, Scribe::Session &session, std::string_view source)
{
    if (source.empty())
        return session.findMissingAssets();

    const auto *scene = engine.getActiveScene();
    if (!scene)
        return {};

    auto references = scene->findAssetReferences(source);
    if (references.empty())
        return {};

    return {{std::string(source), std::move(references)}};
}
}

bool EditorShell::beginReferenceRepair(Context &ctx, Engine &engine, Scribe::Session &session, bool manual, std::string_view source)
{
    const auto *project = engine.getProject();
    if (!project || !engine.hasActiveScene() || engine.isPlaying() || session.transformEditActive() || assetDrag.asset.isValid() || assetDrag.ownsInput)
        return false;

    repairSource = source;
    repairGroups = repairGroupsFor(engine, session, repairSource);
    if (repairGroups.empty() && !manual)
        return false;

    repairGeneration = engine.getSceneGeneration();
    repairProject = project->getProjectFilePath();
    referenceRepairOpen = true;
    missingCheckPending = false;
    checkedMissingGeneration = repairGeneration;
    session.endMerge();

    viewportPanelUI.updateMarquee(ctx, sceneNode, false, repairGeneration);
    assetPreviewPosition.reset();
    assetFeedback.clear();

    if (const auto *window = engine.getWindow(); window && window->getWindow())
        glfwSetInputMode(window->getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    Core::Input::resetMouseTracking();

    showReferenceRepair(ctx, engine);
    ctx.clearEvents();
    uiEvents.clear();
    return true;
}

void EditorShell::showReferenceRepair(Context &ctx, Engine &engine)
{
    const auto *scene = engine.getActiveScene();
    assert(scene);

    const std::string sceneName = scene->getName().empty() ? "Untitled" : scene->getName();
    const std::string scope = "Scene: " + sceneName + (scene->getResourcePath() ? "  (" + *scene->getResourcePath() + ")" : "  (unsaved)");

    const bool empty = repairGroups.empty();
    exitDialog.show(ctx, repairSource.empty() ? "Missing Assets" : "Resolve References", scope, empty ? "" : "Replace", empty ? "" : "Clear References",
                    empty ? "Close" : "Leave Unresolved");

    exitDialog.setNotice( ctx, "Current scene only. Import outside files before replacing.");

    repairGroup = -1;
    repairTargets.clear();

    if (empty)
    {
        const std::vector<std::string> lines{"No unresolved references remain in this view."};
        exitDialog.setDetails(ctx, lines);
        return;
    }

    std::vector<std::string> paths;
    paths.reserve(repairGroups.size());

    for (const auto &group : repairGroups)
    {
        paths.push_back(group.path + "  (" + std::to_string(group.references.size()) + (group.references.size() == 1 ? " reference)" : " references)"));
    }

    exitDialog.setChoices(ctx, 0, "Asset", paths);
    showRepairGroup(ctx, engine);
}

void EditorShell::showRepairGroup(Context &ctx, Engine &engine)
{
    repairGroup = exitDialog.selectedChoice(ctx, 0);
    if (repairGroup < 0 ||
        static_cast<std::size_t>(repairGroup) >= repairGroups.size())
        return;

    const auto &group = repairGroups[repairGroup];
    const auto *scene = engine.getActiveScene();
    const auto *project = engine.getProject();

    std::vector<std::string> lines;
    for (const auto &reference : group.references)
    {
        const auto entity = scene->findEntity(reference.entity);
        const auto *name = entity ? entity->try_get<Scene::NameComponent>() : nullptr;

        const std::string label = name && !name->name.empty() ? name->name : "Entity " + Scene::entityIdToString(reference.entity);

        lines.push_back(label + "  (" + reference.component + "." + reference.field + ")");
    }

    std::sort(lines.begin(), lines.end());
    lines.insert(lines.begin(), "Affected fields in this scene:");
    exitDialog.setDetails(ctx, lines);

    repairTargets = {""};
    const auto &components = engine.getComponentRegistry();

    for (const auto &asset : engine.getAssetManager().getRegistry().getRecords())
    {
        if (asset.missing || asset.path == group.path)
            continue;

        const bool compatible = std::all_of(
            group.references.begin(), group.references.end(),
            [&](const Scene::AssetReference &reference) {
                const auto *component = components.find(reference.component);
                const auto *field = component
                    ? Scene::findField(*component, reference.field)
                    : nullptr;

                return field && field->type == Scene::FieldType::AssetRef &&
                    field->assetType == asset.type;
            });

        if (!compatible)
            continue;

        const auto file = project->resolveResourcePath(asset.path);
        std::error_code error;
        if (file && std::filesystem::is_regular_file(*file, error) && !error)
            repairTargets.push_back(asset.path);
    }

    std::sort(repairTargets.begin() + 1, repairTargets.end());

    auto labels = repairTargets;
    labels.front() = repairTargets.size() == 1 ? "No compatible project assets" : "Choose a replacement...";

    exitDialog.setChoices(ctx, 1, "Replacement", labels);
    exitDialog.setPrimaryEnabled(ctx, false);
    exitDialog.setError(ctx, {});
}

void EditorShell::finishReferenceRepair(Context &ctx, Engine &engine, Scribe::Session &session)
{
    referenceRepairOpen = false;
    repairGroups.clear();
    repairTargets.clear();
    exitDialog.hide(ctx);
    Core::Input::resetMouseTracking();

    if (deletingAsset.isValid())
        showAssetDelete(ctx, engine, session);
}

void EditorShell::handleReferenceRepair(Context &ctx, Engine &engine, Scribe::Session &session)
{
    const auto *project = engine.getProject();
    if (!project || project->getProjectFilePath() != repairProject || engine.getSceneGeneration() != repairGeneration || !engine.hasActiveScene() || engine.isPlaying() ||
        session.transformEditActive())
    {
        deletingAsset = {};
        finishReferenceRepair(ctx, engine, session);
        missingCheckPending = true;
        ctx.clearEvents();

        contentBrowserUI.reportError(ctx, "The displayed scene changed. No repair was applied.");
        return;
    }

    const auto choice = exitDialog.pollChoice(ctx);
    ctx.clearEvents();

    if (choice == DialogChoice::Cancel)
    {
        finishReferenceRepair(ctx, engine, session);
        return;
    }

    if (repairGroups.empty())
        return;

    const int selectedGroup = exitDialog.selectedChoice(ctx, 0);
    if (selectedGroup != repairGroup)
    {
        showRepairGroup(ctx, engine);
        return;
    }

    if (repairGroup < 0 || static_cast<std::size_t>(repairGroup) >= repairGroups.size())
        return;

    const int target = exitDialog.selectedChoice(ctx, 1);
    const bool canReplace = target > 0 && static_cast<std::size_t>(target) < repairTargets.size();

    exitDialog.setPrimaryEnabled(ctx, canReplace);

    if (choice != DialogChoice::Primary && choice != DialogChoice::Secondary)
        return;
    if (choice == DialogChoice::Primary && !canReplace)
        return;

    const auto &group = repairGroups[repairGroup];
    const auto current = engine.getActiveScene()->findAssetReferences(group.path);

    const bool unchanged = std::is_permutation(
        current.begin(), current.end(),
        group.references.begin(), group.references.end(),
        [](const Scene::AssetReference &a, const Scene::AssetReference &b) {
            return a.entity == b.entity &&
                a.component == b.component &&
                a.field == b.field &&
                a.path == b.path;
        });

    if (!unchanged)
    {
        repairGroups = repairGroupsFor(engine, session, repairSource);
        showReferenceRepair(ctx, engine);
        exitDialog.setError(ctx, "References changed. Review the list before applying a repair.");
        return;
    }

    const std::string replacement = choice == DialogChoice::Primary ? repairTargets[target] : "";

    const auto result = session.repairReferences(repairGeneration, group.path, replacement);

    if (!result.ok())
    {
        exitDialog.setError(ctx, result.error);
        return;
    }

    repairGroups = repairGroupsFor(engine, session, repairSource);
    if (repairGroups.empty())
        finishReferenceRepair(ctx, engine, session);
    else
        showReferenceRepair(ctx, engine);
}
} // namespace Cthulhu::Editor