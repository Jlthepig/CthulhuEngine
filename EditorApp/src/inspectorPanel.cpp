#include "inspectorPanel.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "components.hpp"
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
        constexpr double degreesPerRadian = 57.29577951308232;

        double toInspectorUnits(double value, Scene::FieldHint hint)
        {
            return hint == Scene::FieldHint::Degrees ? value * degreesPerRadian : value;
        }

        double fromInspectorUnits(double value, Scene::FieldHint hint)
        {
            return hint == Scene::FieldHint::Degrees ? value / degreesPerRadian : value;
        }

        bool isNumericField(Scene::FieldType type)
        {
            return type == Scene::FieldType::Float || type == Scene::FieldType::Int ||
                   type == Scene::FieldType::Vec3 || type == Scene::FieldType::Color;
        }

        bool choiceMatches(Scene::FieldType type, const Scene::FieldValue &left,
                           const Scene::FieldValue &right)
        {
            switch (type)
            {
            case Scene::FieldType::Enum:
                return std::get<int>(left) == std::get<int>(right);
            case Scene::FieldType::AssetRef:
            case Scene::FieldType::String:
                return std::get<std::string>(left) == std::get<std::string>(right);
            case Scene::FieldType::EntityRef:
                return std::get<Scene::EntityId>(left) == std::get<Scene::EntityId>(right);
            default:
                return false;
            }
        }

        int findChoiceIndex(Scene::FieldType type, const std::vector<Scene::FieldValue> &options,
                            const Scene::FieldValue &value)
        {
            for (std::size_t i = 0; i < options.size(); ++i)
            {
                if (choiceMatches(type, options[i], value))
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        NodeHandle inspectorRow(Context &ctx, NodeHandle parent, std::string_view label)
        {
            const NodeHandle row = ctx.createContainer(parent);
            auto rowLayout = ctx.layoutStyle(row);
            rowLayout.mode = LayoutMode::Horizontal;
            rowLayout.horizontalAlignment = Alignment::Stretch;
            rowLayout.verticalAlignment = Alignment::Center;
            rowLayout.gap = 4.0f;
            rowLayout.minSize = Vec2{0.0f, 28.0f};
            rowLayout.preferredSize = Vec2{0.0f, 28.0f};

            const NodeHandle name = ctx.createLabel(row, label);
            auto nameLayout = ctx.layoutStyle(name);
            nameLayout.minSize = Vec2{72.0f, 26.0f};
            nameLayout.preferredSize = Vec2{72.0f, 26.0f};
            nameLayout.maxSize = Vec2{72.0f, 26.0f};
            nameLayout.verticalAlignment = Alignment::Center;
            return row;
        }

        NodeHandle transformInspectorRow(Context &ctx, NodeHandle parent, std::string_view label)
        {
            const NodeHandle row = ctx.createContainer(parent);
            auto rowLayout = ctx.layoutStyle(row);
            rowLayout.mode = LayoutMode::Horizontal;
            rowLayout.horizontalAlignment = Alignment::Stretch;
            rowLayout.verticalAlignment = Alignment::Center;
            rowLayout.gap = 4.0f;
            rowLayout.minSize = Vec2{0.0f, 30.0f};
            rowLayout.preferredSize = Vec2{0.0f, 30.0f};

            const NodeHandle name = ctx.createLabel(row, label);
            ctx.style(name).all.text = ctx.theme().textMuted;
            auto nameLayout = ctx.layoutStyle(name);
            nameLayout.minSize = Vec2{72.0f, 30.0f};
            nameLayout.preferredSize = Vec2{72.0f, 30.0f};
            nameLayout.maxSize = Vec2{72.0f, 30.0f};
            return row;
        }

        Color transformAxisColor(std::size_t axis)
        {
            constexpr Color colors[] = {
                Color::fromRGBA8(235, 96, 96),
                Color::fromRGBA8(120, 215, 100),
                Color::fromRGBA8(96, 150, 245)};
            return colors[axis];
        }

        std::string transformFieldLabel(std::string_view field)
        {
            if (field == "position") return "Position";
            if (field == "rotation") return "Rotation";
            if (field == "scale") return "Scale";
            return std::string(field);
        }

        void fitInspectorControl(Context &ctx, NodeHandle control)
        {
            auto layout = ctx.layoutStyle(control);
            layout.minSize = Vec2{0.0f, 26.0f};
            layout.preferredSize = Vec2{0.0f, 26.0f};
            layout.maxSize = Vec2{InfiniteLayoutSize, 26.0f};
            layout.horizontalAlignment = Alignment::Stretch;
            layout.verticalAlignment = Alignment::Center;
        }

        void setVectorAxis(glm::vec3 &value, std::size_t axis, float component)
        {
            if (axis == 0) value.x = component;
            else if (axis == 1) value.y = component;
            else value.z = component;
        }

        float getVectorAxis(const glm::vec3 &value, std::size_t axis)
        {
            if (axis == 0) return value.x;
            if (axis == 1) return value.y;
            return value.z;
        }
    } // namespace


    void InspectorPanel::build(Context &ctx, NodeHandle panelNode)
    {
        panel = panelNode;
        content = ctx.createContainer(panel);
        auto layout = ctx.layoutStyle(content);
        layout.mode = LayoutMode::Vertical;
        layout.horizontalAlignment = Alignment::Stretch;
        layout.verticalAlignment = Alignment::Stretch;
        layout.gap = 4.0f;
        layout.scrollY = true;
        resetContent(ctx);
    }

    void InspectorPanel::resetContent(Context &ctx)
    {
        if (ctx.isValid(generatedContent))
        {
            ctx.destroyNode(generatedContent);
        }
        generatedContent = ctx.createContainer(content);
        auto layout = ctx.layoutStyle(generatedContent);
        layout.mode = LayoutMode::Vertical;
        layout.horizontalAlignment = Alignment::Stretch;
        layout.verticalAlignment = Alignment::Start;
        layout.fitContentY = true;
        layout.gap = 4.0f;
    }

    void InspectorPanel::update(Context &ctx, Engine &engine, Scribe::Session &session,
                                Scene::Scene *scene, std::optional<Scene::EntityId> entityId)
    {
        selectedEntity = entityId;
        std::vector<std::string> attached;
        if (scene && selectedEntity)
        {
            const auto entity = scene->findEntity(*selectedEntity);
            if (entity)
            {
                for (const Scene::ComponentDescriptor &component : engine.getComponentRegistry().getAll())
                {
                    if (component.has(*entity))
                    {
                        attached.push_back(component.name);
                    }
                }
            }
            else
            {
                selectedEntity.reset();
            }
        }

        if (!initialized || builtEntity != selectedEntity || attached != componentNames)
        {
            if (initialized)
            {
                session.endMerge();
            }
            rebuild(ctx, engine, scene);
        }

        syncTransform(ctx, engine, session);
        processEvents(ctx, engine, session);
        refreshValues(ctx, engine);
    }

    void InspectorPanel::rebuild(Context &ctx, Engine &engine, Scene::Scene *scene)
    {
        resetContent(ctx);
        bindings.clear();
        componentNames.clear();
        builtEntity = selectedEntity;

        const auto addMessage = [&](std::string_view message) {
            [[maybe_unused]] const NodeHandle label = ctx.createLabel(generatedContent, message);
        };

        if (!scene || !selectedEntity)
        {
            addMessage("Select an entity in the Hierarchy");
            initialized = true;
            return;
        }

        auto entity = scene->findEntity(*selectedEntity);
        if (!entity)
        {
            selectedEntity.reset();
            builtEntity.reset();
            addMessage("Selected entity no longer exists");
            initialized = true;
            return;
        }

        std::vector<std::pair<Scene::EntityId, std::string>> sceneEntities;
        const auto collectEntities = [&](auto &&self, std::optional<Scene::EntityId> parentId) -> void {
            const std::vector<Scene::EntityId> children =
                parentId ? scene->getChildren(*parentId) : scene->getRootEntities();
            for (const Scene::EntityId id : children)
            {
                const auto currentEntity = scene->findEntity(id);
                if (!currentEntity)
                {
                    continue;
                }
                const auto *name = currentEntity->try_get<Scene::NameComponent>();
                sceneEntities.emplace_back(id, name ? name->name : "Entity");
                self(self, id);
            }
        };
        collectEntities(collectEntities, std::nullopt);

        const Scene::ComponentRegistry &registry = engine.getComponentRegistry();
        for (const Scene::ComponentDescriptor &component : registry.getAll())
        {
            if (!component.has(*entity))
            {
                continue;
            }
            componentNames.push_back(component.name);

            const bool isTransform = component.name == "Transform";
            const NodeHandle section = ctx.createCollapsibleSection(generatedContent, component.name, false);
            const NodeHandle content = ctx.collapsibleSectionContent(section);
            auto sectionLayout = ctx.layoutStyle(content);
            sectionLayout.mode = LayoutMode::Vertical;
            sectionLayout.horizontalAlignment = Alignment::Stretch;
            sectionLayout.gap = isTransform ? 6.0f : 3.0f;
            sectionLayout.padding = isTransform ? Padding{} : Padding{4.0f, 3.0f};

            for (const Scene::FieldDescriptor &field : component.fields)
            {
                const auto currentValue = registry.getField(*entity, component.name, field.name);
                if (!currentValue)
                {
                    continue;
                }

                const auto bind = [&](NodeHandle control, std::size_t axis,
                                      std::vector<Scene::FieldValue> options = {}) {
                    const double shown = isNumericField(field.type) ? ctx.numericValue(control) : 0.0;
                    bindings.push_back({control, component.name, field.name, field.type,
                                                 field.hint, axis, shown, std::move(options)});
                };

                if (field.type == Scene::FieldType::Bool)
                {
                    const NodeHandle control =
                        ctx.createCheckbox(content, field.name, std::get<bool>(*currentValue));
                    fitInspectorControl(ctx, control);
                    bind(control, 0);
                    continue;
                }

                if (field.type == Scene::FieldType::Float || field.type == Scene::FieldType::Int ||
                    field.type == Scene::FieldType::Vec3 || field.type == Scene::FieldType::Color)
                {
                    const bool isVector = field.type == Scene::FieldType::Vec3 ||
                                          field.type == Scene::FieldType::Color;
                    const std::string label = isTransform ? transformFieldLabel(field.name) : field.name;
                    const NodeHandle row = isTransform ? transformInspectorRow(ctx, content, label)
                                                       : inspectorRow(ctx, content, label);
                    const auto addNumeric = [&](NodeHandle parent, double value, std::size_t axis) {
                        NodeHandle numericParent = parent;
                        if (isVector && !isTransform)
                        {
                            numericParent = ctx.createContainer(parent);
                            auto axisLayout = ctx.layoutStyle(numericParent);
                            axisLayout.mode = LayoutMode::Horizontal;
                            axisLayout.horizontalAlignment = Alignment::Stretch;
                            axisLayout.verticalAlignment = Alignment::Center;
                            axisLayout.gap = 2.0f;
                        }
                        if (isVector)
                        {
                            constexpr std::string_view axisNames[] = {"X", "Y", "Z"};
                            const NodeHandle axisLabel = ctx.createLabel(numericParent, axisNames[axis]);
                            auto labelLayout = ctx.layoutStyle(axisLabel);
                            const float rowHeight = isTransform ? 30.0f : 22.0f;
                            labelLayout.minSize = Vec2{12.0f, rowHeight};
                            labelLayout.preferredSize = Vec2{12.0f, rowHeight};
                            labelLayout.maxSize = Vec2{12.0f, rowHeight};
                            if (isTransform)
                            {
                                ctx.style(axisLabel).all.text = transformAxisColor(axis);
                            }
                        }

                        const NumericInputType inputType = field.type == Scene::FieldType::Int
                                                               ? NumericInputType::Integer
                                                               : NumericInputType::Float;
                        const NodeHandle control = ctx.createNumericInput(
                            numericParent, toInspectorUnits(value, field.hint), inputType);
                        double minimum = field.min < field.max
                                             ? field.min
                                             : -std::numeric_limits<float>::max();
                        double maximum = field.min < field.max
                                             ? field.max
                                             : std::numeric_limits<float>::max();
                        if (field.type == Scene::FieldType::Int)
                        {
                            minimum = std::max(minimum, static_cast<double>(std::numeric_limits<int>::lowest()));
                            maximum = std::min(maximum, static_cast<double>(std::numeric_limits<int>::max()));
                        }
                        ctx.setNumericRange(control, toInspectorUnits(minimum, field.hint),
                                            toInspectorUnits(maximum, field.hint));
                        const float defaultStep = field.type == Scene::FieldType::Int ? 1.0f : 0.1f;
                        const float step = field.step > 0.0f ? field.step : defaultStep;
                        const double displayStep = isTransform && field.name == "rotation" && field.step <= 0.0f
                                                       ? 1.0
                                                       : toInspectorUnits(step, field.hint);
                        ctx.setNumericStep(control, displayStep);
                        if (isTransform)
                        {
                            auto controlLayout = ctx.layoutStyle(control);
                            controlLayout.minSize = Vec2{44.0f, 30.0f};
                            controlLayout.preferredSize = Vec2{0.0f, 30.0f};
                            controlLayout.maxSize = Vec2{InfiniteLayoutSize, 30.0f};
                            controlLayout.horizontalAlignment = Alignment::Stretch;
                            controlLayout.verticalAlignment = Alignment::Center;
                        }
                        else
                        {
                            fitInspectorControl(ctx, control);
                        }
                        bind(control, axis);
                    };

                    if (isVector)
                    {
                        const glm::vec3 value = std::get<glm::vec3>(*currentValue);
                        for (std::size_t axis = 0; axis < 3; ++axis)
                        {
                            addNumeric(row, getVectorAxis(value, axis), axis);
                        }
                    }
                    else if (field.type == Scene::FieldType::Float)
                    {
                        addNumeric(row, std::get<float>(*currentValue), 0);
                    }
                    else
                    {
                        addNumeric(row, static_cast<double>(std::get<int>(*currentValue)), 0);
                    }
                    continue;
                }

                if (field.type == Scene::FieldType::String)
                {
                    const NodeHandle row = inspectorRow(ctx, content, field.name);
                    const NodeHandle control =
                        ctx.createTextInput(row, std::get<std::string>(*currentValue));
                    fitInspectorControl(ctx, control);
                    bind(control, 0);
                    continue;
                }

                std::vector<std::string> labels;
                std::vector<Scene::FieldValue> options;
                if (field.type == Scene::FieldType::Enum)
                {
                    for (std::size_t i = 0; i < field.enumNames.size(); ++i)
                    {
                        labels.push_back(field.enumNames[i]);
                        options.emplace_back(static_cast<int>(i));
                    }
                }
                else if (field.type == Scene::FieldType::AssetRef)
                {
                    labels.emplace_back("<None>");
                    options.emplace_back(std::string{});
                    const auto &records = engine.getAssetManager().getRegistry().getRecords();
                    for (const Assets::AssetRecord &record : records)
                    {
                        if (record.type != field.assetType)
                        {
                            continue;
                        }
                        labels.push_back(record.path + (record.missing ? " (missing)" : ""));
                        options.emplace_back(record.path);
                    }
                    const std::string &currentPath = std::get<std::string>(*currentValue);
                    if (!currentPath.empty() && findChoiceIndex(field.type, options, *currentValue) < 0)
                    {
                        labels.push_back("<Missing> " + currentPath);
                        options.emplace_back(currentPath);
                    }
                }
                else if (field.type == Scene::FieldType::EntityRef)
                {
                    labels.emplace_back("<None>");
                    options.emplace_back(Scene::EntityId{});
                    for (const auto &[id, name] : sceneEntities)
                    {
                        labels.push_back(name + " (" + Scene::entityIdToString(id) + ")");
                        options.emplace_back(id);
                    }
                    const Scene::EntityId currentId = std::get<Scene::EntityId>(*currentValue);
                    if (currentId.isValid() && findChoiceIndex(field.type, options, *currentValue) < 0)
                    {
                        labels.push_back("<Missing entity> " + Scene::entityIdToString(currentId));
                        options.emplace_back(currentId);
                    }
                }
                else
                {
                    continue;
                }

                const NodeHandle row = inspectorRow(ctx, content, field.name);
                const int selectedIndex = findChoiceIndex(field.type, options, *currentValue);
                const NodeHandle control = ctx.createComboBox(row, labels, selectedIndex);
                fitInspectorControl(ctx, control);
                bind(control, 0, std::move(options));
            }
        }

        initialized = true;
    }

    void InspectorPanel::processEvents(Context &ctx, Engine &engine, Scribe::Session &session)
    {
        octogui::UIEvent event;
        while (ctx.pollEvent(event))
        {
            const auto binding = std::find_if(
                bindings.begin(), bindings.end(),
                [&](const Binding &candidate) { return candidate.control == event.target; });
            if (binding == bindings.end())
            {
                continue;
            }

            if (event.type == octogui::UIEventType::FocusLost ||
                event.type == octogui::UIEventType::FocusGained)
            {
                session.endMerge();
                continue;
            }
            if (event.type == octogui::UIEventType::PointerReleased && isNumericField(binding->type))
            {
                session.endMerge();
                continue;
            }
            if (event.type == octogui::UIEventType::ValueChanged &&
                binding->component == "Transform" && isNumericField(binding->type))
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
            const auto current = engine.getComponentRegistry().getField(
                *entity, binding->component, binding->field);
            if (!current)
            {
                continue;
            }

            std::optional<Scene::FieldValue> nextValue;
            bool discreteEdit = false;
            if (event.type == octogui::UIEventType::ValueChanged)
            {
                if (binding->type == Scene::FieldType::Float)
                {
                    const float value = static_cast<float>(fromInspectorUnits(
                        ctx.numericValue(binding->control), binding->hint));
                    nextValue = Scene::FieldValue{value};
                }
                else if (binding->type == Scene::FieldType::Int)
                {
                    nextValue = Scene::FieldValue{static_cast<int>(std::llround(
                        fromInspectorUnits(ctx.numericValue(binding->control), binding->hint)))};
                }
                else if (binding->type == Scene::FieldType::Vec3 ||
                         binding->type == Scene::FieldType::Color)
                {
                    glm::vec3 value = std::get<glm::vec3>(*current);
                    const float component = static_cast<float>(fromInspectorUnits(
                        ctx.numericValue(binding->control), binding->hint));
                    setVectorAxis(value, binding->axis, component);
                    nextValue = Scene::FieldValue{value};
                }
                else if (binding->type == Scene::FieldType::String)
                {
                    nextValue = Scene::FieldValue{std::string(ctx.text(binding->control))};
                }
            }
            else if (event.type == octogui::UIEventType::CheckedChanged &&
                     binding->type == Scene::FieldType::Bool)
            {
                nextValue = Scene::FieldValue{ctx.isChecked(binding->control)};
                discreteEdit = true;
            }
            else if (event.type == octogui::UIEventType::SelectionChanged &&
                     (binding->type == Scene::FieldType::Enum ||
                      binding->type == Scene::FieldType::AssetRef ||
                      binding->type == Scene::FieldType::EntityRef))
            {
                const int index = ctx.comboBoxSelectedIndex(binding->control);
                if (index >= 0 && static_cast<std::size_t>(index) < binding->options.size())
                {
                    nextValue = binding->options[static_cast<std::size_t>(index)];
                    discreteEdit = true;
                }
            }

            if (!nextValue)
            {
                continue;
            }

            const Scribe::Result result = session.setField(
                *selectedEntity, binding->component, binding->field, *nextValue);
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
            const double modelValue = toInspectorUnits(
                getVectorAxis(std::get<glm::vec3>(*current), binding.axis), binding.hint);
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

                const Scribe::Result result = session.setField(
                    *selectedEntity, binding.component, binding.field, Scene::FieldValue{updated});
                if (!result.ok())
                {
                    Log::Print("TRANSFORM EDIT FAILED: " + result.error, "Editor", LogType::LOG_ERROR);
                }
                continue;
            }

            if (modelValue != binding.shownValue && !ctx.isFocused(binding.control) &&
                !ctx.isActive(binding.control))
            {
                ctx.setNumericValue(binding.control, modelValue);
                binding.shownValue = ctx.numericValue(binding.control);
            }
        }
    }

    void InspectorPanel::refreshValues(Context &ctx, Engine &engine)
    {
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
                const double value = toInspectorUnits(
                    getVectorAxis(std::get<glm::vec3>(*current), binding.axis), binding.hint);
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
            else if (binding.type == Scene::FieldType::Enum ||
                     binding.type == Scene::FieldType::AssetRef ||
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
