#include "inspectorPanel.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "engine.hpp"
#include "log_utils.hpp"
#include "scene.hpp"
#include "session.hpp"

using namespace octogui;
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Editor
{
namespace
{
void setVectorAxis(glm::vec3 &value, std::size_t axis, float component)
{
    if (axis == 0)
        value.x = component;
    else if (axis == 1)
        value.y = component;
    else
        value.z = component;
}
} // namespace

void InspectorPanel::processEvents(Context &ctx, Engine &engine, Scribe::Session &session, std::span<const octogui::UIEvent> events, bool assetsChanged)
{
    for (const auto &event : events)
    {
        if (boundGeneration != engine.getSceneGeneration())
            return;

        const auto removeBinding =
            std::find_if(removeBindings.begin(), removeBindings.end(), [&](const RemoveBinding &candidate) {
                return candidate.button == event.target;
            });
        if (removeBinding != removeBindings.end() && event.type == octogui::UIEventType::Clicked)
        {
            if (!engine.isPlaying() && selectedEntity)
            {
                const auto result = session.removeComponent(*selectedEntity, removeBinding->component);
                if (!result.ok())
                {
                    Log::Print("REMOVE COMPONENT FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
                }
                else
                {
                    session.endMerge();
                }
            }
            continue;
        }

        if (event.target == addComponentButton && event.type == octogui::UIEventType::Clicked)
        {
            if (!engine.isPlaying() && selectedEntity && ctx.isValid(addComponentCombo))
            {
                const int index = ctx.comboBoxSelectedIndex(addComponentCombo);
                if (index >= 0 && static_cast<std::size_t>(index) < availableComponents.size())
                {
                    const auto result = session.addComponent(*selectedEntity, availableComponents[static_cast<std::size_t>(index)]);
                    if (!result.ok())
                    {
                        Log::Print("ADD COMPONENT FAILED: " + result.error,"Editor", LogType::LOG_ERROR);
                    }
                    else
                    {
                        session.endMerge();
                    }
                }
            }
            continue;
        }
        if (event.target == addComponentCombo)
        {
            if (event.type == octogui::UIEventType::SelectionChanged)
            {
                continue;
            }
        }

        const auto binding = std::find_if(bindings.begin(), bindings.end(), [&](const Binding &candidate) {
            return candidate.control == event.target;
        });
        if (binding == bindings.end())
        {
            continue;
        }

        if (event.type == octogui::UIEventType::FocusLost || event.type == octogui::UIEventType::FocusGained)
        {
            session.endMerge();
            continue;
        }
        if (event.type == octogui::UIEventType::PointerReleased && isNumericField(binding->type))
        {
            session.endMerge();
            continue;
        }
        if (event.type == octogui::UIEventType::ValueChanged && binding->component == "Transform" &&
            isNumericField(binding->type))
        {
            continue;
        }
        // Discard choices displayed before the asset change.
        if (assetsChanged && binding->type == Scene::FieldType::AssetRef)
        {
            continue;
        }
        if (engine.isPlaying() || !selectedEntity)
        {
            continue;
        }

        Scene::Scene *scene = engine.getActiveScene();
        std::optional<flecs::entity> entity;
        if (scene)
        {
            entity = scene->findEntity(*selectedEntity);
        }
        if (!entity)
        {
            continue;
        }
        const auto current = engine.getComponentRegistry().getField(*entity, binding->component, binding->field);
        if (!current)
        {
            continue;
        }

        std::optional<Scene::FieldValue> nextValue;
        bool discreteEdit = false;
        if (event.type == octogui::UIEventType::ValueChanged &&
            (binding->type == Scene::FieldType::Float || binding->type == Scene::FieldType::Int ||
             binding->type == Scene::FieldType::Vec3 || binding->type == Scene::FieldType::Color ||
             binding->type == Scene::FieldType::String))
        {
            if (binding->type == Scene::FieldType::Float)
            {
                const float value =
                    static_cast<float>(fromInspectorUnits(ctx.numericValue(binding->control), binding->hint));
                nextValue = Scene::FieldValue{value};
            }
            else if (binding->type == Scene::FieldType::Int)
            {
                nextValue = Scene::FieldValue{static_cast<int>(
                    std::llround(fromInspectorUnits(ctx.numericValue(binding->control), binding->hint)))};
            }
            else if (binding->type == Scene::FieldType::Vec3 || binding->type == Scene::FieldType::Color)
            {
                glm::vec3 value = std::get<glm::vec3>(*current);
                const float component =
                    static_cast<float>(fromInspectorUnits(ctx.numericValue(binding->control), binding->hint));
                setVectorAxis(value, binding->axis, component);
                nextValue = Scene::FieldValue{value};
            }
            else if (binding->type == Scene::FieldType::String)
            {
                nextValue = Scene::FieldValue{std::string(ctx.text(binding->control))};
            }
        }
        else if (event.type == octogui::UIEventType::CheckedChanged && binding->type == Scene::FieldType::Bool)
        {
            nextValue = Scene::FieldValue{ctx.isChecked(binding->control)};
            discreteEdit = true;
        }
        else if ((event.type == octogui::UIEventType::ValueChanged ||
                  event.type == octogui::UIEventType::SelectionChanged) &&
                 (binding->type == Scene::FieldType::Enum || binding->type == Scene::FieldType::AssetRef ||
                  binding->type == Scene::FieldType::EntityRef))
        {
            const int index = ctx.comboBoxSelectedIndex(binding->control);
            if (index >= 0 && static_cast<std::size_t>(index) < binding->options.size())
            {
                nextValue = binding->options[static_cast<std::size_t>(index)];
                discreteEdit = true;
            }
        }

        if (!nextValue || (binding->type == Scene::FieldType::AssetRef && *nextValue == *current))
        {
            continue;
        }

        const Scribe::Result result = session.setField(*selectedEntity, binding->component, binding->field, *nextValue);
        if (!result.ok())
        {
            Log::Print("INSPECTOR EDIT FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
        }
        if (discreteEdit)
        {
            session.endMerge();
        }
    }
}

void InspectorPanel::syncTransform(Context &ctx, Engine &engine, Scribe::Session &session)
{
    if (boundGeneration != engine.getSceneGeneration())
        return;

    if (!selectedEntity)
    {
        return;
    }

    Scene::Scene *scene = engine.getActiveScene();
    std::optional<flecs::entity> entity;
    if (scene)
    {
        entity = scene->findEntity(*selectedEntity);
    }
    if (!entity)
    {
        return;
    }

    const Scene::ComponentRegistry &registry = engine.getComponentRegistry();
    for (Binding &binding : bindings)
    {
        if (binding.component != "Transform" ||
            (binding.type != Scene::FieldType::Vec3 && binding.type != Scene::FieldType::Color))
        {
            continue;
        }

        const auto current = registry.getField(*entity, binding.component, binding.field);
        if (!current)
        {
            continue;
        }
        const double modelValue =
            toInspectorUnits(getVectorAxis(std::get<glm::vec3>(*current), binding.axis), binding.hint);
        const double uiValue = ctx.numericValue(binding.control);

        if (engine.isPlaying())
        {
            if (uiValue != modelValue)
            {
                ctx.setNumericValue(binding.control, modelValue);
            }
            binding.shownValue = ctx.numericValue(binding.control);
            continue;
        }

        if (uiValue != binding.shownValue)
        {
            binding.shownValue = uiValue;
            glm::vec3 updated = std::get<glm::vec3>(*current);
            const float storedValue = static_cast<float>(fromInspectorUnits(uiValue, binding.hint));
            setVectorAxis(updated, binding.axis, storedValue);

            const Scribe::Result result =
                session.setField(*selectedEntity, binding.component, binding.field, Scene::FieldValue{updated});
            if (!result.ok())
            {
                Log::Print("TRANSFORM EDIT FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
            }
            continue;
        }

        if (modelValue != binding.shownValue && !ctx.isFocused(binding.control) && !ctx.isActive(binding.control))
        {
            ctx.setNumericValue(binding.control, modelValue);
            binding.shownValue = ctx.numericValue(binding.control);
        }
    }
}

void InspectorPanel::refreshAssetChoices(Context &ctx, Engine &engine)
{
    Scene::Scene *scene = engine.getActiveScene();
    if (!scene || !selectedEntity) return;

    const auto entity = scene->findEntity(*selectedEntity);
    if (!entity) return;

    const auto &components = engine.getComponentRegistry();
    const auto &assets = engine.getAssetManager().getRegistry();

    for (Binding &binding : bindings)
    {
        if (binding.type != Scene::FieldType::AssetRef) continue;

        const auto *component = components.find(binding.component);
        const auto *field = component ? Scene::findField(*component, binding.field) : nullptr;
        const auto current = components.getField(*entity, binding.component, binding.field);
        if (!field || !current) continue;

        std::vector<std::string> labels;
        assetChoices(assets, field->assetType, *current, labels, binding.options);
        ctx.setComboBoxItems(binding.control, labels);
        ctx.setComboBoxSelectedIndex(binding.control, findChoiceIndex(binding.type, binding.options, *current));
    }
}

void InspectorPanel::refreshValues(Context &ctx, Engine &engine)
{
    if (boundGeneration != engine.getSceneGeneration())
        return;

    const bool playing = engine.isPlaying();
    for (const Binding &binding : bindings)
    {
        ctx.setEnabled(binding.control, !playing);
    }
    if (!selectedEntity)
    {
        return;
    }

    Scene::Scene *scene = engine.getActiveScene();
    std::optional<flecs::entity> entity;
    if (scene)
    {
        entity = scene->findEntity(*selectedEntity);
    }
    if (!entity)
    {
        return;
    }

    const Scene::ComponentRegistry &registry = engine.getComponentRegistry();
    for (const Binding &binding : bindings)
    {
        if (binding.component == "Transform" && isNumericField(binding.type))
        {
            continue;
        }
        if (!playing && (ctx.isFocused(binding.control) || ctx.isActive(binding.control)))
        {
            continue;
        }
        const auto current = registry.getField(*entity, binding.component, binding.field);
        if (!current)
        {
            continue;
        }

        if (binding.type == Scene::FieldType::Float)
        {
            const double value = toInspectorUnits(std::get<float>(*current), binding.hint);
            if (std::abs(ctx.numericValue(binding.control) - value) > 0.000001)
            {
                ctx.setNumericValue(binding.control, value);
            }
        }
        else if (binding.type == Scene::FieldType::Int)
        {
            const double value = std::get<int>(*current);
            if (std::abs(ctx.numericValue(binding.control) - value) > 0.000001)
            {
                ctx.setNumericValue(binding.control, value);
            }
        }
        else if (binding.type == Scene::FieldType::Vec3 || binding.type == Scene::FieldType::Color)
        {
            const double value =
                toInspectorUnits(getVectorAxis(std::get<glm::vec3>(*current), binding.axis), binding.hint);
            if (std::abs(ctx.numericValue(binding.control) - value) > 0.000001)
            {
                ctx.setNumericValue(binding.control, value);
            }
        }
        else if (binding.type == Scene::FieldType::Bool)
        {
            const bool value = std::get<bool>(*current);
            if (ctx.isChecked(binding.control) != value)
            {
                ctx.setChecked(binding.control, value);
            }
        }
        else if (binding.type == Scene::FieldType::String)
        {
            const std::string &value = std::get<std::string>(*current);
            if (ctx.text(binding.control) != value)
            {
                ctx.setText(binding.control, value);
            }
        }
        else if (binding.type == Scene::FieldType::Enum || binding.type == Scene::FieldType::AssetRef ||
                 binding.type == Scene::FieldType::EntityRef)
        {
            const int index = findChoiceIndex(binding.type, binding.options, *current);
            if (ctx.comboBoxSelectedIndex(binding.control) != index)
            {
                ctx.setComboBoxSelectedIndex(binding.control, index);
            }
        }
    }
}
} // namespace Cthulhu::Editor
