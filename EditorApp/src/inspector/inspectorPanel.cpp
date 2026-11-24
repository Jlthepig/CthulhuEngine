#include "inspectorPanel.hpp"
#include "editorMetrics.hpp"
#include "editorPanels.hpp"

#include <algorithm>
#include <cmath>
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

constexpr float MinLabelWidth = 64.0f;
constexpr float MaxLabelWidth = 128.0f;
constexpr float AxisChipWidth = 14.0f;
constexpr float MinNumericWidth = 40.0f;

// Turns descriptor names into display text << "maxWalkableSlope" -> "Max Walkable Slope",
// Scribe calls and saved scenes keep the original names.
std::string prettyLabel(std::string_view name)
{
    if (name == "fov")
        return "FOV";

    const auto isUpper = [](char c) { return c >= 'A' && c <= 'Z'; };
    const auto isLower = [](char c) { return c >= 'a' && c <= 'z'; };
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };

    std::string out;
    out.reserve(name.size() + 4);
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const char c = name[i];
        if (c == '_')
        {
            if (!out.empty() && out.back() != ' ')
                out.push_back(' ');
            continue;
        }

        if (i > 0 && !out.empty() && out.back() != ' ')
        {
            const char previous = name[i - 1];
            const bool nextLower = i + 1 < name.size() && isLower(name[i + 1]);
            const bool wordStart = (isUpper(c) && (!isUpper(previous) || nextLower)) ||
                                   (isDigit(c) && !isDigit(previous));
            if (wordStart)
                out.push_back(' ');
        }

        out.push_back(out.empty() && isLower(c) ? static_cast<char>(c - 'a' + 'A') : c);
    }
    return out;
}

NodeHandle inspectorRow(Context &ctx, NodeHandle parent, std::string_view label, float labelWidth,
                        std::vector<NodeHandle> &labels)
{
    const NodeHandle row = ctx.createContainer(parent);
    auto rowLayout = ctx.layoutStyle(row);
    rowLayout.mode = LayoutMode::Horizontal;
    rowLayout.horizontalAlignment = Alignment::Stretch;
    rowLayout.verticalAlignment = Alignment::Center;
    rowLayout.gap = SpaceS;
    rowLayout.minSize = Vec2{0.0f, ControlHeight};
    rowLayout.preferredSize = Vec2{0.0f, ControlHeight};

    const NodeHandle name = ctx.createLabel(row, prettyLabel(label));
    ctx.setTextAlign(name, TextAlign::Start, TextAlign::Center);
    ctx.style(name).all.text = ctx.theme().textMuted;
    auto nameLayout = ctx.layoutStyle(name);
    nameLayout.minSize = Vec2{labelWidth, ControlHeight};
    nameLayout.preferredSize = Vec2{labelWidth, ControlHeight};
    nameLayout.maxSize = Vec2{labelWidth, ControlHeight};
    nameLayout.verticalAlignment = Alignment::Center;
    nameLayout.clip = true;
    labels.push_back(name);
    return row;
}

Color transformAxisColor(std::size_t axis)
{
    constexpr Color colors[] = {Color::fromRGBA8(235, 96, 96),
                                Color::fromRGBA8(120, 215, 100),
                                Color::fromRGBA8(96, 150, 245)};
    return colors[axis];
}

void fitInspectorControl(Context &ctx, NodeHandle control)
{
    auto layout = ctx.layoutStyle(control);
    layout.minSize = Vec2{80.0f, ControlHeight};
    layout.preferredSize = Vec2{0.0f, ControlHeight};
    layout.maxSize = Vec2{InfiniteLayoutSize, ControlHeight};
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
    layout.gap = SpaceS;
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
    layout.gap = SpaceS;
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
    const bool playing = engine.isPlaying();

    if (initialized && boundGeneration == generation && inputGeneration == generation && boundPlaying == playing)
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

    if (!initialized || boundGeneration != generation || boundPlaying != playing || builtEntity != selectedEntity || attached != componentNames)
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

    if (ctx.isValid(entityNameLabel) && scene && selectedEntity)
    {
        if (const auto entity = scene->findEntity(*selectedEntity))
        {
            const auto *name = entity->try_get<Scene::NameComponent>();
            const std::string_view text = name ? std::string_view{name->name} : std::string_view{"Entity"};
            if (ctx.text(entityNameLabel) != text)
                ctx.setText(entityNameLabel, text);
        }
    }

    const float available = ctx.tree().rect(generatedContent).w - SpaceS - SpaceM;
    if (available > 0.0f)
    {
        const float vectorRow = 3.0f * (AxisChipWidth + MinNumericWidth) + 6.0f * SpaceS;
        const float width = std::clamp(std::floor(available - vectorRow), MinLabelWidth, MaxLabelWidth);
        if (width != labelWidth)
        {
            labelWidth = width;
            for (const NodeHandle label : labelNodes)
            {
                auto layout = ctx.layoutStyle(label);
                layout.minSize = Vec2{width, ControlHeight};
                layout.preferredSize = Vec2{width, ControlHeight};
                layout.maxSize = Vec2{width, ControlHeight};
            }
        }
    }
}

void InspectorPanel::rebuild(Context &ctx, Engine &engine, Scene::Scene *scene)
{
    resetContent(ctx);
    boundGeneration = engine.getSceneGeneration();
    boundPlaying = engine.isPlaying();
    availableComponents.clear();
    addComponentCombo = {};
    addComponentButton = {};
    bindings.clear();
    removeBindings.clear();
    componentNames.clear();
    labelNodes.clear();
    entityNameLabel = {};
    builtEntity = selectedEntity;

    const auto addMessage = [&](std::string_view message) {
        const NodeHandle box = ctx.createContainer(generatedContent);
        auto boxLayout = ctx.layoutStyle(box);
        boxLayout.mode = LayoutMode::Vertical;
        boxLayout.horizontalAlignment = Alignment::Center;
        boxLayout.gap = SpaceM;
        boxLayout.padding = Padding{0.0f, 2.0f * SpaceXl, 0.0f, 0.0f};
        boxLayout.fitContentY = true;

        const NodeHandle icon = ctx.createIcon(box, "cursor");
        ctx.setIconSize(icon, 2.0f * IconSize);
        ctx.setIconColor(icon, ctx.theme().textDisabled);
        auto iconLayout = ctx.layoutStyle(icon);
        iconLayout.minSize = Vec2{2.0f * IconSize, 2.0f * IconSize};
        iconLayout.preferredSize = Vec2{2.0f * IconSize, 2.0f * IconSize};
        iconLayout.maxSize = Vec2{2.0f * IconSize, 2.0f * IconSize};

        const NodeHandle label = ctx.createLabel(box, message);
        ctx.style(label).all.text = ctx.theme().textMuted;
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

    {
        const NodeHandle header = ctx.createContainer(generatedContent);
        auto headerLayout = ctx.layoutStyle(header);
        headerLayout.mode = LayoutMode::Horizontal;
        headerLayout.verticalAlignment = Alignment::Center;
        headerLayout.gap = SpaceM;
        headerLayout.padding = Padding{SpaceS, 0.0f};
        headerLayout.minSize = Vec2{0.0f, RowHeight};
        headerLayout.preferredSize = Vec2{0.0f, RowHeight};
        headerLayout.clip = true;

        const auto *name = entity->try_get<Scene::NameComponent>();
        entityNameLabel = ctx.createLabel(header, name ? std::string_view{name->name} : std::string_view{"Entity"});
        ctx.setFontSlot(entityNameLabel, FontSlot::Secondary);

        const NodeHandle idLabel = ctx.createLabel(header, "ID " + Scene::entityIdToString(*selectedEntity));
        ctx.style(idLabel).all.text = ctx.theme().textDisabled;
    }

    const Scene::ComponentRegistry &registry = engine.getComponentRegistry();
    for (const Scene::ComponentDescriptor &component : registry.getAll())
    {
        if (!component.has(*entity))
        {
            continue;
        }
        componentNames.push_back(component.name);

        const bool isTransform = component.name == "Transform";
        const NodeHandle section = ctx.createCollapsibleSection(generatedContent, prettyLabel(component.name), false);
        ctx.setFontSlot(section, FontSlot::Secondary);
        const NodeHandle content = ctx.collapsibleSectionContent(section);
        auto sectionLayout = ctx.layoutStyle(content);
        sectionLayout.mode = LayoutMode::Vertical;
        sectionLayout.horizontalAlignment = Alignment::Stretch;
        sectionLayout.gap = SpaceXs;
        sectionLayout.padding = Padding{SpaceS, SpaceXs, SpaceM, SpaceXs};

        NodeHandle firstRow{};
        bool firstRowIsVector = false;
        const auto row = [&](std::string_view label, bool vector) {
            const NodeHandle created = inspectorRow(ctx, content, label, labelWidth, labelNodes);
            if (!ctx.isValid(firstRow))
            {
                firstRow = created;
                firstRowIsVector = vector;
            }
            return created;
        };

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
                const NodeHandle control = ctx.createCheckbox(row(field.name, false), "", std::get<bool>(*currentValue));
                fitInspectorControl(ctx, control);
                bind(control, 0);
                continue;
            }

            if (field.type == Scene::FieldType::Float || field.type == Scene::FieldType::Int ||
                field.type == Scene::FieldType::Vec3 || field.type == Scene::FieldType::Color)
            {
                const bool isVector = field.type == Scene::FieldType::Vec3 || field.type == Scene::FieldType::Color;
                const NodeHandle numericRow = row(field.name, isVector);
                const auto addNumeric = [&](NodeHandle parent, double value, std::size_t axis) {
                    NodeHandle numericParent = parent;
                    if (isVector)
                    {
                        constexpr std::string_view axisNames[] = {"X", "Y", "Z"};
                        const NodeHandle axisLabel = ctx.createLabel(numericParent, axisNames[axis]);
                        auto labelLayout = ctx.layoutStyle(axisLabel);
                        labelLayout.minSize = Vec2{AxisChipWidth, ControlHeight};
                        labelLayout.preferredSize = Vec2{AxisChipWidth, ControlHeight};
                        labelLayout.maxSize = Vec2{AxisChipWidth, ControlHeight};
                
                        auto chip = ctx.style(axisLabel);
                        chip.all.text = transformAxisColor(axis);
                        chip.all.background = transformAxisColor(axis).withAlpha(0.2f);
                        chip.cornerRadius = ctx.theme().smallRadius;
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
                        controlLayout.minSize = Vec2{MinNumericWidth, ControlHeight};
                        controlLayout.preferredSize = Vec2{0.0f, ControlHeight};
                        controlLayout.maxSize = Vec2{InfiniteLayoutSize, ControlHeight};
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
                        addNumeric(numericRow, getVectorAxis(value, axis), axis);
                    }
                }
                else if (field.type == Scene::FieldType::Float)
                {
                    addNumeric(numericRow, std::get<float>(*currentValue), 0);
                }
                else
                {
                    addNumeric(numericRow, static_cast<double>(std::get<int>(*currentValue)), 0);
                }
                continue;
            }

            if (field.type == Scene::FieldType::String)
            {
                const NodeHandle control = ctx.createTextInput(row(field.name, false), std::get<std::string>(*currentValue));
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

            const int selectedIndex = findChoiceIndex(options, *currentValue);
            const NodeHandle control = ctx.createComboBox(row(field.name, false), labels, selectedIndex);
            fitInspectorControl(ctx, control);
            bind(control, 0, std::move(options));
        }

        if (!component.core)
        {
            NodeHandle removeParent = firstRow;
            if (!ctx.isValid(removeParent) || firstRowIsVector)
            {
                removeParent = ctx.createContainer(content);
                auto removeRowLayout = ctx.layoutStyle(removeParent);
                removeRowLayout.mode = LayoutMode::Horizontal;
                removeRowLayout.horizontalAlignment = Alignment::End;
                removeRowLayout.verticalAlignment = Alignment::Center;
                removeRowLayout.minSize = Vec2{0.0f, IconButtonSize};
                removeRowLayout.preferredSize = Vec2{0.0f, IconButtonSize};
            }

            const NodeHandle removeButton = ctx.createButton(removeParent, "");
            auto buttonLayout = ctx.layoutStyle(removeButton);
            buttonLayout.mode = LayoutMode::Horizontal;
            buttonLayout.horizontalAlignment = Alignment::Center;
            buttonLayout.verticalAlignment = Alignment::Center;
            buttonLayout.padding = Padding{};
            buttonLayout.minSize = Vec2{IconButtonSize, IconButtonSize};
            buttonLayout.preferredSize = Vec2{IconButtonSize, IconButtonSize};
            buttonLayout.maxSize = Vec2{IconButtonSize, IconButtonSize};
            styleGhostButton(ctx, removeButton);
            ctx.setTooltip(removeButton, "Remove component");
            const NodeHandle trashIcon = ctx.createIcon(removeButton, "trash");
            ctx.setIconSize(trashIcon, IconSize);
            ctx.setIconColor(trashIcon, ctx.theme().textMuted);
            ctx.setEnabled(removeButton, !engine.isPlaying());
            removeBindings.push_back({removeButton, component.name});
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

        const NodeHandle spacer = ctx.createContainer(generatedContent);
        auto spacerLayout = ctx.layoutStyle(spacer);
        spacerLayout.minSize = Vec2{0.0f, SpaceS};
        spacerLayout.preferredSize = Vec2{0.0f, SpaceS};
        spacerLayout.maxSize = Vec2{InfiniteLayoutSize, SpaceS};

        const NodeHandle addRow = ctx.createContainer(generatedContent);
        auto rowLayout = ctx.layoutStyle(addRow);
        rowLayout.mode = LayoutMode::Horizontal;
        rowLayout.horizontalAlignment = Alignment::Stretch;
        rowLayout.verticalAlignment = Alignment::Center;
        rowLayout.gap = SpaceS;
        rowLayout.minSize = Vec2{0.0f, ControlHeight};
        rowLayout.preferredSize = Vec2{0.0f, ControlHeight};

        if (availableComponents.empty())
        {
            addComponentCombo = ctx.createComboBox(addRow, std::vector<std::string>{"<No components to add>"}, 0);
        }
        else
        {
            std::vector<std::string> displayNames;
            displayNames.reserve(availableComponents.size());
            for (const auto &name : availableComponents)
                displayNames.push_back(prettyLabel(name));
            addComponentCombo = ctx.createComboBox(addRow, displayNames, 0);
        }

        fitInspectorControl(ctx, addComponentCombo);

        addComponentButton = ctx.createButton(addRow, "Add");
        auto buttonLayout = ctx.layoutStyle(addComponentButton);
        buttonLayout.minSize = Vec2{48.0f, ControlHeight};
        buttonLayout.preferredSize = Vec2{48.0f, ControlHeight};
        buttonLayout.maxSize = Vec2{48.0f, ControlHeight};
        {
            const Theme &theme = ctx.theme();
            auto style = ctx.style(addComponentButton);
            style.borderWidth = 0.0f;
            style.normal.background = theme.accent;
            style.normal.text = theme.accentText;
            style.hovered.background = theme.accentHovered;
            style.hovered.text = theme.accentText;
            style.active.background = theme.accentActive;
            style.active.text = theme.accentText;
        }

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
