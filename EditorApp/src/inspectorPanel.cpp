#include "inspectorPanel.hpp"

#include <algorithm>
#include <limits>

#include "components.hpp"
#include "engine.hpp"
#include "scene.hpp"
#include "session.hpp"

using namespace octogui;

namespace Cthulhu::Editor
{
namespace
{
constexpr double degreesPerRadian = 57.29577951308232;

NodeHandle inspectorRow(Context &ctx, NodeHandle parent, std::string_view label)
{
    const NodeHandle row = ctx.createContainer(parent);
    auto rowLayout = ctx.layoutStyle(row);
    rowLayout.mode = LayoutMode::Horizontal;
    rowLayout.horizontalAlignment = Alignment::Stretch;
    rowLayout.verticalAlignment = Alignment::Center;
    rowLayout.gap = 4.0f;
    rowLayout.minSize = Vec2{0.0f, 30.0f};
    rowLayout.preferredSize = Vec2{0.0f, 30.0f};

    const NodeHandle name =
        ctx.createLabel(row, label == "modelPath" || label == "filePath" ? std::string_view{"Path"} : label);
    auto nameLayout = ctx.layoutStyle(name);
    nameLayout.minSize = Vec2{86.0f, 26.0f};
    nameLayout.preferredSize = Vec2{86.0f, 26.0f};
    nameLayout.maxSize = Vec2{86.0f, 26.0f};
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
    constexpr Color colors[] = {Color::fromRGBA8(235, 96, 96),
                                Color::fromRGBA8(120, 215, 100),
                                Color::fromRGBA8(96, 150, 245)};
    return colors[axis];
}

std::string transformFieldLabel(std::string_view field)
{
    if (field == "position")
        return "Position";
    if (field == "rotation")
        return "Rotation";
    if (field == "scale")
        return "Scale";
    return std::string(field);
}

void fitInspectorControl(Context &ctx, NodeHandle control)
{
    auto layout = ctx.layoutStyle(control);
    layout.minSize = Vec2{80.0f, 30.0f};
    layout.preferredSize = Vec2{0.0f, 30.0f};
    layout.maxSize = Vec2{InfiniteLayoutSize, 30.0f};
    layout.horizontalAlignment = Alignment::Stretch;
    layout.verticalAlignment = Alignment::Center;
}

} // namespace

double InspectorPanel::toInspectorUnits(double value, Scene::FieldHint hint)
{
    return hint == Scene::FieldHint::Degrees ? value * degreesPerRadian : value;
}

double InspectorPanel::fromInspectorUnits(double value, Scene::FieldHint hint)
{
    return hint == Scene::FieldHint::Degrees ? value / degreesPerRadian : value;
}

bool InspectorPanel::isNumericField(Scene::FieldType type)
{
    return type == Scene::FieldType::Float || type == Scene::FieldType::Int || type == Scene::FieldType::Vec3 ||
           type == Scene::FieldType::Color;
}

int InspectorPanel::findChoiceIndex(const std::vector<Scene::FieldValue> &options, const Scene::FieldValue &value)
{
    const auto found = std::find(options.begin(), options.end(), value);
    return found == options.end()
        ? -1
        : static_cast<int>(found - options.begin());
}

void InspectorPanel::assetChoices(const Assets::AssetRegistry &registry,
                                  Assets::AssetType type,
                                  const Scene::FieldValue &current,
                                  std::vector<std::string> &labels,
                                  std::vector<Scene::FieldValue> &options)
{
    labels.clear();
    options.clear();
    labels.emplace_back("<None>");
    options.emplace_back(std::string{});

    for (const Assets::AssetRecord &record : registry.getRecords())
    {
        if (record.type != type) continue;
        labels.push_back(record.path + (record.missing ? " (missing)" : ""));
        options.emplace_back(record.path);
    }

    const std::string &path = std::get<std::string>(current);
    if (!path.empty() && findChoiceIndex(options, current) < 0)
    {
        labels.push_back("<Missing> " + path);
        options.emplace_back(path);
    }
}

float InspectorPanel::getVectorAxis(const glm::vec3 &value, std::size_t axis)
{
    if (axis == 0)
        return value.x;
    if (axis == 1)
        return value.y;
    return value.z;
}

void InspectorPanel::build(Context &ctx, NodeHandle panelNode)
{
    content = ctx.createContainer(panelNode);
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

void InspectorPanel::update(Context &ctx,
                            Engine &engine,
                            Scribe::Session &session,
                            Scene::Scene *scene,
                            std::optional<Scene::EntityId> entityId,
                            std::span<const octogui::UIEvent> events,
                            std::uint64_t inputGeneration,
                            bool assetsChanged)
{
    const std::uint64_t generation = engine.getSceneGeneration();

    if (initialized && boundGeneration == generation && inputGeneration == generation)
    {
        syncTransform(ctx, engine, session);
        processEvents(ctx, engine, session, events, assetsChanged);
    }

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

    if (!initialized || boundGeneration != generation || builtEntity != selectedEntity || attached != componentNames)
    {
        if (initialized)
        {
            session.endMerge();
        }
        rebuild(ctx, engine, scene);
    }
    else if (assetsChanged)
    {
        refreshAssetChoices(ctx, engine);
    }

    syncTransform(ctx, engine, session);
    refreshValues(ctx, engine);
}

void InspectorPanel::rebuild(Context &ctx, Engine &engine, Scene::Scene *scene)
{
    resetContent(ctx);
    boundGeneration = engine.getSceneGeneration();
    availableComponents.clear();
    addComponentCombo = {};
    addComponentButton = {};
    bindings.clear();
    removeBindings.clear();
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
        const std::vector<Scene::EntityId> children = parentId ? scene->getChildren(*parentId)
                                                               : scene->getRootEntities();
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
        sectionLayout.gap = 6.0f;
        sectionLayout.padding = isTransform ? Padding{0.0f, 0.0f, 6.0f, 0.0f} : Padding{4.0f, 3.0f, 6.0f, 3.0f};

        if (!component.core)
        {
            const NodeHandle removeRow = ctx.createContainer(content);
            auto removeRowLayout = ctx.layoutStyle(removeRow);
            removeRowLayout.mode = LayoutMode::Horizontal;
            removeRowLayout.horizontalAlignment = Alignment::End;
            removeRowLayout.verticalAlignment = Alignment::Center;
            removeRowLayout.gap = 6.0f;
            removeRowLayout.minSize = Vec2{0.0f, 18.0f};
            removeRowLayout.preferredSize = Vec2{0.0f, 18.0f};

            const NodeHandle removeButton = ctx.createButton(removeRow, "");
            auto buttonLayout = ctx.layoutStyle(removeButton);
            buttonLayout.mode = LayoutMode::Horizontal;
            buttonLayout.horizontalAlignment = Alignment::Center;
            buttonLayout.verticalAlignment = Alignment::Center;
            buttonLayout.padding = Padding{};
            buttonLayout.minSize = Vec2{24.0f, 24.0f};
            buttonLayout.preferredSize = Vec2{24.0f, 24.0f};
            buttonLayout.maxSize = Vec2{24.0f, 24.0f};
            const NodeHandle trashIcon = ctx.createIcon(removeButton, "trash");
            ctx.setIconSize(trashIcon, 14.0f);
            ctx.setIconColor(trashIcon, ctx.theme().textMuted);
            ctx.setEnabled(removeButton, !engine.isPlaying());
            removeBindings.push_back({removeButton, component.name});
        }

        for (const Scene::FieldDescriptor &field : component.fields)
        {
            if (field.hint == Scene::FieldHint::Hidden)
            {
                continue;
            }

            const auto currentValue = registry.getField(*entity, component.name, field.name);
            if (!currentValue)
            {
                continue;
            }

            const auto bind = [&](NodeHandle control, std::size_t axis, std::vector<Scene::FieldValue> options = {}) {
                const double shown = isNumericField(field.type) ? ctx.numericValue(control) : 0.0;
                bindings.push_back(
                    {control, component.name, field.name, field.type, field.hint, axis, shown, std::move(options)});
            };

            if (field.type == Scene::FieldType::Bool)
            {
                const NodeHandle control = ctx.createCheckbox(content, field.name, std::get<bool>(*currentValue));
                fitInspectorControl(ctx, control);
                bind(control, 0);
                continue;
            }

            if (field.type == Scene::FieldType::Float || field.type == Scene::FieldType::Int ||
                field.type == Scene::FieldType::Vec3 || field.type == Scene::FieldType::Color)
            {
                const bool isVector = field.type == Scene::FieldType::Vec3 || field.type == Scene::FieldType::Color;
                const std::string label = isTransform ? transformFieldLabel(field.name) : field.name;
                const NodeHandle row = isTransform ? transformInspectorRow(ctx, content, label)
                                                   : inspectorRow(ctx, content, label);
                const auto addNumeric = [&](NodeHandle parent, double value, std::size_t axis) {
                    NodeHandle numericParent = parent;
                    if (isVector)
                    {
                        constexpr std::string_view axisNames[] = {"X", "Y", "Z"};
                        const NodeHandle axisLabel = ctx.createLabel(numericParent, axisNames[axis]);
                        auto labelLayout = ctx.layoutStyle(axisLabel);
                        const float rowHeight = isTransform ? 30.0f : 28.0f;
                        labelLayout.minSize = Vec2{12.0f, rowHeight};
                        labelLayout.preferredSize = Vec2{12.0f, rowHeight};
                        labelLayout.maxSize = Vec2{12.0f, rowHeight};
                        ctx.style(axisLabel).all.text = transformAxisColor(axis);
                    }

                    const NumericInputType inputType = field.type == Scene::FieldType::Int ? NumericInputType::Integer
                                                                                           : NumericInputType::Float;
                    const NodeHandle control =
                        ctx.createNumericInput(numericParent, toInspectorUnits(value, field.hint), inputType);
                    double minimum = field.min < field.max ? field.min : -std::numeric_limits<float>::max();
                    double maximum = field.min < field.max ? field.max : std::numeric_limits<float>::max();
                    if (field.type == Scene::FieldType::Int)
                    {
                        minimum = std::max(minimum, static_cast<double>(std::numeric_limits<int>::lowest()));
                        maximum = std::min(maximum, static_cast<double>(std::numeric_limits<int>::max()));
                    }
                    ctx.setNumericRange(control,
                                        toInspectorUnits(minimum, field.hint),
                                        toInspectorUnits(maximum, field.hint));
                    const float defaultStep = field.type == Scene::FieldType::Int ? 1.0f : 0.1f;
                    const float step = field.step > 0.0f ? field.step : defaultStep;
                    const double displayStep = isTransform && field.name == "rotation" && field.step <= 0.0f
                                                   ? 1.0
                                                   : toInspectorUnits(step, field.hint);
                    ctx.setNumericStep(control, displayStep);
                    if (isTransform || isVector)
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
                const NodeHandle control = ctx.createTextInput(row, std::get<std::string>(*currentValue));
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
                assetChoices(engine.getAssetManager().getRegistry(),
                             field.assetType, *currentValue, labels, options);
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
                if (currentId.isValid() && findChoiceIndex(options, *currentValue) < 0)
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
            const int selectedIndex = findChoiceIndex(options, *currentValue);
            const NodeHandle control = ctx.createComboBox(row, labels, selectedIndex);
            fitInspectorControl(ctx, control);
            bind(control, 0, std::move(options));
        }
    }

    if (scene && selectedEntity)
    {
        for (const auto &descriptor : registry.getAll())
        {
            if (descriptor.core)
            {
                continue;
            }
            if (descriptor.has(*entity))
            {
                continue;
            }
            availableComponents.push_back(descriptor.name);
        }

        const NodeHandle separator = ctx.createSeparator(generatedContent);
        ctx.setSeparatorOrientation(separator, SeparatorOrientation::Horizontal);

        const NodeHandle addRow = ctx.createContainer(generatedContent);
        auto rowLayout = ctx.layoutStyle(addRow);
        rowLayout.mode = LayoutMode::Horizontal;
        rowLayout.horizontalAlignment = Alignment::Stretch;
        rowLayout.verticalAlignment = Alignment::Center;
        rowLayout.gap = 6.0f;
        rowLayout.minSize = Vec2{0.0f, 28.0f};
        rowLayout.preferredSize = Vec2{0.0f, 28.0f};

        if (availableComponents.empty())
        {
            addComponentCombo = ctx.createComboBox(addRow, std::vector<std::string>{"<No components to add>"}, 0);
        }
        else
        {
            addComponentCombo = ctx.createComboBox(addRow, availableComponents, 0);
        }

        fitInspectorControl(ctx, addComponentCombo);

        addComponentButton = ctx.createButton(addRow, "Add");
        auto buttonLayout = ctx.layoutStyle(addComponentButton);
        buttonLayout.minSize = Vec2{48.0f, 26.0f};
        buttonLayout.preferredSize = Vec2{48.0f, 26.0f};
        buttonLayout.maxSize = Vec2{48.0f, 26.0f};

        const bool canAdd = !availableComponents.empty() && !engine.isPlaying();
        ctx.setEnabled(addComponentCombo, canAdd);
        ctx.setEnabled(addComponentButton, canAdd);
        ctx.setEnabled(addRow, !engine.isPlaying());
    }
    else
    {
        addComponentCombo = {};
        addComponentButton = {};
    }

    initialized = true;
}

} // namespace Cthulhu::Editor
