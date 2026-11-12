#include "inspectorPanel.hpp"

#include <cassert>
#include <filesystem>
#include <utility>

#include "assetRegistry.hpp"
#include "engine.hpp"
#include "log_utils.hpp"
#include "project.hpp"
#include "session.hpp"

namespace Cthulhu::Editor
{
AssetDropFeedback InspectorPanel::assetDrop(octogui::Context &ctx, Engine &engine, Scribe::Session &session,
                                            const Assets::AssetRecord &asset, bool commit, octogui::DrawList &feedback)
{
    AssetDropFeedback result;
    if (!initialized || boundGeneration != engine.getSceneGeneration() || builtEntity != selectedEntity || !selectedEntity ||
        engine.isPlaying() || session.transformEditActive())
        return result;

    auto *scene = engine.getActiveScene();
    const auto entity = scene ? scene->findEntity(*selectedEntity) : std::nullopt;
    if (!entity || asset.missing) return result;

    auto &registry = engine.getComponentRegistry();
    const auto reject = octogui::Color::fromRGBA8(224, 90, 90);

    for (const Binding &binding : bindings)
    {
        if (binding.type != Scene::FieldType::AssetRef || !ctx.isValid(binding.control) || !ctx.isEnabled(binding.control))
            continue;

        const auto *component = registry.find(binding.component);
        const auto *field = component ? Scene::findField(*component, binding.field) : nullptr;
        if (!field || !component->has(*entity)) continue;

        const bool compatible = field->assetType == asset.type;
        const bool hovered = ctx.isHovered(binding.control);
        const auto rect = ctx.visibleRect(binding.control);
        assert(!hovered || rect.contains(ctx.input().mouse.position));

        if (!rect.isEmpty() && (compatible || hovered))
        {
            feedback.addRectOutline(rect.shrink(1.0f), compatible ? ctx.theme().accent : reject, hovered ? 2.0f : 1.0f);
        }

        if (!hovered) continue;
        if (!compatible)
        {
            result.message = "Expected " + std::string(Assets::assetTypeName(field->assetType)) + " asset.";
            return result;
        }

        result.accepted = true;
        result.message = "Release to assign.";
        if (!commit) return result;

        const auto *project = engine.getProject();
        const std::string path = asset.path;
        const auto file = project ? project->resolveResourcePath(path) : std::nullopt;

        std::error_code error;
        if (!file || !std::filesystem::is_regular_file(*file, error) || error)
            return {false, "Asset file is unavailable. Refresh the browser.", true};

        session.endMerge();
        auto applied = session.setField(*selectedEntity, binding.component, binding.field, Scene::FieldValue{path});
        session.endMerge();

        if (!applied.ok())
        {
            KalaHeaders::KalaLog::Log::Print("ASSET DROP FAILED: " + applied.error, "Editor", KalaHeaders::KalaLog::LogType::LOG_ERROR);
            return {false, std::move(applied.error), true};
        }

        refreshAssetChoices(ctx, engine);
        refreshValues(ctx, engine);
        ctx.setFocused(binding.control);

        result.message = applied.status == Scribe::Result::Status::NoChange ? "Already assigned." : "Assigned.";
        return result;
    }
    return result;
}
} // namespace Cthulhu::Editor