#include "projectInputEditor.hpp"
#include "editorMetrics.hpp"
#include "editorTheme.hpp"

#include <algorithm>
#include <cctype>

#include <GLFW/glfw3.h>

#include "session.hpp"

namespace Cthulhu::Editor
{
using namespace octogui;
using namespace Project;

namespace
{
constexpr std::array controlLabels{
    "Move Forward", "Move Backward", "Move Left", "Move Right", "Jump",
    "Fire", "Reload", "Interact", "Sprint", "Crouch"
};
static_assert(controlLabels.size() == inputControlCount);

NodeHandle row(Context &ctx, NodeHandle parent)
{
    const auto node = ctx.createContainer(parent);
    auto layout = ctx.layoutStyle(node);
    layout.mode = LayoutMode::Horizontal;
    layout.verticalAlignment = Alignment::Center;
    layout.preferredSize = Vec2{0.0f, 32.0f};
    layout.gap = SpaceM;
    return node;
}

NodeHandle column(Context &ctx, NodeHandle parent)
{
    const auto node = ctx.createContainer(parent);
    auto layout = ctx.layoutStyle(node);
    layout.mode = LayoutMode::Vertical;
    layout.horizontalAlignment = Alignment::Stretch;
    layout.fitContentY = true;
    layout.gap = SpaceS;
    return node;
}

void noticeText(Context &ctx, NodeHandle node, std::string_view text)
{
    ctx.setText(node, text);
    ctx.setTooltip(node, text);
    ctx.setVisible(node, !text.empty());
}

std::string bindingLabel(InputBinding binding)
{
    const auto key = binding.code;
    if (binding.device == InputDevice::MouseButton)
    {
        constexpr std::array names{"Left Mouse", "Right Mouse", "Middle Mouse"};
        return key >= 0 && key < static_cast<int>(names.size())
            ? names[static_cast<std::size_t>(key)] : "Mouse " + std::to_string(key + 1);
    }

    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25)
        return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL)
    {
        constexpr std::array names{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                                   ".", "/", "*", "-", "+", "Enter", "="};
        return "Numpad " + std::string(names[static_cast<std::size_t>(key - GLFW_KEY_KP_0)]);
    }

    switch (key)
    {
    case GLFW_KEY_SPACE: return "Space";
    case GLFW_KEY_TAB: return "Tab";
    case GLFW_KEY_ENTER: return "Enter";
    case GLFW_KEY_BACKSPACE: return "Backspace";
    case GLFW_KEY_INSERT: return "Insert";
    case GLFW_KEY_DELETE: return "Delete";
    case GLFW_KEY_RIGHT: return "Right";
    case GLFW_KEY_LEFT: return "Left";
    case GLFW_KEY_DOWN: return "Down";
    case GLFW_KEY_UP: return "Up";
    case GLFW_KEY_PAGE_UP: return "Page Up";
    case GLFW_KEY_PAGE_DOWN: return "Page Down";
    case GLFW_KEY_HOME: return "Home";
    case GLFW_KEY_END: return "End";
    case GLFW_KEY_CAPS_LOCK: return "Caps Lock";
    case GLFW_KEY_SCROLL_LOCK: return "Scroll Lock";
    case GLFW_KEY_NUM_LOCK: return "Num Lock";
    case GLFW_KEY_PRINT_SCREEN: return "Print Screen";
    case GLFW_KEY_PAUSE: return "Pause";
    case GLFW_KEY_LEFT_SHIFT: return "Left Shift";
    case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
    case GLFW_KEY_LEFT_CONTROL: return "Left Ctrl";
    case GLFW_KEY_RIGHT_CONTROL: return "Right Ctrl";
    case GLFW_KEY_LEFT_ALT: return "Left Alt";
    case GLFW_KEY_RIGHT_ALT: return "Right Alt";
    case GLFW_KEY_LEFT_SUPER: return "Left Super";
    case GLFW_KEY_RIGHT_SUPER: return "Right Super";
    case GLFW_KEY_MENU: return "Menu";
    default: break;
    }

    if (const char *label = glfwGetKeyName(key, 0))
        return label;
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        return std::string(1, static_cast<char>(key));
    return "Key " + std::to_string(key);
}

NodeHandle iconButton(Context &ctx, NodeHandle parent, std::string_view icon, std::string_view tooltip)
{
    const auto button = ctx.createButton(parent, "");
    auto layout = ctx.layoutStyle(button);
    layout.mode = LayoutMode::Horizontal;
    layout.minSize = Vec2{IconButtonSize, IconButtonSize};
    layout.preferredSize = Vec2{IconButtonSize, IconButtonSize};
    layout.maxSize = Vec2{IconButtonSize, IconButtonSize};
    layout.horizontalAlignment = Alignment::Center;
    layout.verticalAlignment = Alignment::Center;
    styleGhostButton(ctx, button);

    const auto image = ctx.createIcon(button, icon);
    ctx.setIconSize(image, IconSize);
    ctx.setIconColor(image, ctx.theme().textMuted);
    ctx.setTooltip(button, tooltip);
    return button;
}

NodeHandle mutedLabel(Context &ctx, NodeHandle parent, std::string_view text)
{
    const auto label = ctx.createLabel(parent, text);
    ctx.style(label).all.text = ctx.theme().textMuted;
    ctx.layoutStyle(label).clip = true;
    return label;
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    for (auto &c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

void ProjectInputEditor::build(Context &ctx, NodeHandle page, NodeHandle overlay, const ProjectInputMap &input)
{
    saved = draft = input;
    renaming = invalidRename = false;
    rejectedName.clear();
    renameError.clear();
    filterText.clear();
    selected = lastClicked = {};
    clickAge = 1.0f;
    actionRows.clear();
    actionList = name = nameError = {};

    const auto toolbar = row(ctx, page);
    filter = ctx.createTextInput(toolbar, "", "Filter actions");
    ctx.layoutStyle(filter).preferredSize = Vec2{};
    ctx.layoutStyle(filter).clip = true;
    newName = ctx.createTextInput(toolbar, "", "New action name");
    ctx.layoutStyle(newName).preferredSize = Vec2{220.0f, 0.0f};
    ctx.layoutStyle(newName).clip = true;
    addAction = ctx.createButton(toolbar, "Add Action");
    revert = ctx.createButton(toolbar, "Revert Changes");
    styleButtonRole(ctx, revert, ButtonRole::Ghost);

    newNameError = ctx.createLabel(page, "");
    ctx.layoutStyle(newNameError).clip = true;
    ctx.style(newNameError).all.text = DangerColor;
    notice = ctx.createLabel(page, "");
    ctx.layoutStyle(notice).clip = true;

    actions = column(ctx, page);
    ctx.layoutStyle(actions).gap = SpaceXs;

    const auto controls = ctx.createCollapsibleSection(page, "Built-in Controls", false);
    const auto controlBody = ctx.collapsibleSectionContent(controls);
    (void)mutedLabel(ctx, controlBody, "Choose which action drives each built-in FPS control.");
    for (std::size_t i = 0; i < assignments.size(); ++i)
    {
        const auto line = row(ctx, controlBody);
        const auto label = mutedLabel(ctx, line, controlLabels[i]);
        ctx.layoutStyle(label).preferredSize = Vec2{128.0f, 0.0f};
        assignments[i] = ctx.createComboBox(line, {});
        noBindings[i] = ctx.createLabel(line, "No bindings");
        ctx.style(noBindings[i]).all.text = WarningColor;
    }

    captureBackdrop = ctx.createPanel(overlay);
    ctx.setZLayer(captureBackdrop, 1);
    auto backdropStyle = ctx.style(captureBackdrop);
    backdropStyle.all.background = Color::fromRGBA8(0, 0, 0, 150);
    backdropStyle.borderWidth = 0.0f;
    backdropStyle.cornerRadius = 0.0f;
    backdropStyle.shadow = ShadowStyle{};
    auto backdropLayout = ctx.layoutStyle(captureBackdrop);
    backdropLayout.mode = LayoutMode::Vertical;
    backdropLayout.horizontalAlignment = Alignment::Center;
    backdropLayout.verticalAlignment = Alignment::Center;
    backdropLayout.padding = Padding{SpaceL};

    const auto card = ctx.createPanel(captureBackdrop);
    ctx.style(card).all.background = ctx.theme().surfaceRaised;
    ctx.style(card).shadow = ctx.theme().elevationHigh;
    ctx.style(card).cornerRadius = 6.0f;
    auto cardLayout = ctx.layoutStyle(card);
    cardLayout.preferredSize = Vec2{380.0f, 0.0f};
    cardLayout.fitContentY = true;
    cardLayout.padding = Padding{SpaceXl};
    cardLayout.gap = SpaceL;

    captureTitle = ctx.createLabel(card, "");
    ctx.setFontSlot(captureTitle, FontSlot::Secondary);
    ctx.layoutStyle(captureTitle).preferredSize = Vec2{0.0f, RowHeight};
    ctx.layoutStyle(captureTitle).clip = true;

    const auto listen = ctx.createPanel(card);
    auto listenStyle = ctx.style(listen);
    listenStyle.all.background = ctx.theme().surface;
    listenStyle.all.border = ctx.theme().focus;
    listenStyle.borderWidth = 1.0f;
    listenStyle.cornerRadius = ctx.theme().controlRadius;
    listenStyle.shadow = ShadowStyle{};
    auto listenLayout = ctx.layoutStyle(listen);
    listenLayout.mode = LayoutMode::Vertical;
    listenLayout.horizontalAlignment = Alignment::Center;
    listenLayout.verticalAlignment = Alignment::Center;
    listenLayout.preferredSize = Vec2{0.0f, 64.0f};

    captureInput = ctx.createLabel(listen, "");
    ctx.setFontSlot(captureInput, FontSlot::Secondary);

    captureMessage = ctx.createLabel(card, "");
    ctx.layoutStyle(captureMessage).preferredSize = Vec2{0.0f, RowHeight};
    ctx.layoutStyle(captureMessage).clip = true;
    const auto hint = mutedLabel(ctx, card, "Press any key or mouse button. Escape cancels.");
    ctx.layoutStyle(hint).preferredSize = Vec2{0.0f, RowHeight};
    ctx.setVisible(captureBackdrop, false);

    noticeText(ctx, newNameError, {});
    showNotice(ctx, {});
    refresh(ctx);
}

std::size_t ProjectInputEditor::selectedIndex() const
{
    const auto iter = std::ranges::find(draft.actions, selected, &InputAction::id);
    return static_cast<std::size_t>(iter - draft.actions.begin());
}

bool ProjectInputEditor::renamePending(const Context &ctx) const
{
    const auto *action = draft.find(selected);
    return renaming && action && ctx.isValid(name) && ctx.text(name) != action->name;
}

void ProjectInputEditor::refresh(Context &ctx)
{
    if (!draft.find(selected))
    {
        selected = draft.actions.empty() ? InputActionId{} : draft.actions[0].id;
        renaming = invalidRename = false;
        renameError.clear();
    }

    std::vector<std::string> choices{"Unassigned"};
    for (const auto &action : draft.actions)
        choices.push_back("  " + action.name);

    for (std::size_t i = 0; i < assignments.size(); ++i)
    {
        const auto action = std::ranges::find(draft.actions, draft.assignments[i], &InputAction::id);
        const bool assigned = action != draft.actions.end();
        ctx.setComboBoxItems(assignments[i], choices);
        ctx.setComboBoxSelectedIndex(assignments[i], assigned ? static_cast<i32>(action - draft.actions.begin() + 1) : 0);
        ctx.setVisible(noBindings[i], assigned && action->bindings.empty());
    }

    const std::string pendingName = renaming && ctx.isValid(name) ? std::string(ctx.text(name)) : std::string{};
    const bool nameFocused = renaming && ctx.isValid(name) && ctx.isFocused(name);

    if (ctx.isValid(actionList))
        ctx.destroyNode(actionList);
    actionRows.clear();
    name = nameError = {};
    actionList = column(ctx, actions);
    ctx.layoutStyle(actionList).gap = SpaceXs;

    if (draft.actions.empty())
    {
        (void)mutedLabel(ctx, actionList, "No actions yet. Name one above and press Add Action.");
        return;
    }

    const auto needle = lowered(filterText);
    for (const auto &action : draft.actions)
    {
        const bool current = action.id == selected;
        const bool editing = renaming && current;
        if (!editing && !needle.empty() && lowered(action.name).find(needle) == std::string::npos)
            continue;

        auto &entry = actionRows.emplace_back();
        entry.id = action.id;

        const auto header = ctx.createPanel(actionList);
        auto headerStyle = ctx.style(header);
        headerStyle.all.background = current ? ctx.theme().surfaceActive : ctx.theme().surface;
        headerStyle.borderWidth = 0.0f;
        headerStyle.cornerRadius = ctx.theme().controlRadius;
        headerStyle.shadow = ShadowStyle{};
        auto headerLayout = ctx.layoutStyle(header);
        headerLayout.mode = LayoutMode::Horizontal;
        headerLayout.verticalAlignment = Alignment::Center;
        headerLayout.preferredSize = Vec2{0.0f, ToolbarHeight};
        headerLayout.padding = Padding{SpaceS, 0.0f};
        headerLayout.gap = SpaceS;

        if (editing)
        {
            name = ctx.createTextInput(header, pendingName.empty() ? std::string_view(action.name) : std::string_view(pendingName), "Action name");
            ctx.layoutStyle(name).preferredSize = Vec2{220.0f, ControlHeight};
            ctx.layoutStyle(name).clip = true;
        }
        else
        {
            entry.label = ctx.createButton(header, action.name);
            styleGhostButton(ctx, entry.label);
            ctx.setTextAlign(entry.label, TextAlign::Start, TextAlign::Center);
            ctx.setFontSlot(entry.label, FontSlot::Secondary);
            auto labelLayout = ctx.layoutStyle(entry.label);
            labelLayout.preferredSize = Vec2{220.0f, ControlHeight};
            labelLayout.padding = Padding{SpaceM, 0.0f};
            labelLayout.clip = true;
            ctx.setTooltip(entry.label, "Double-click or press Enter to rename");
        }

        std::string usedBy;
        for (std::size_t i = 0; i < draft.assignments.size(); ++i)
        {
            if (draft.assignments[i] != action.id)
                continue;
            usedBy += usedBy.empty() ? "Used by " : ", ";
            usedBy += controlLabels[i];
        }
        const auto used = mutedLabel(ctx, header, usedBy);
        ctx.layoutStyle(used).preferredSize = Vec2{};
        ctx.setTooltip(used, usedBy);

        entry.addBinding = iconButton(ctx, header, "plus", "Add binding");
        entry.remove = iconButton(ctx, header, "trash", "Delete action");

        if (editing)
        {
            nameError = ctx.createLabel(actionList, "");
            ctx.layoutStyle(nameError).clip = true;
            ctx.layoutStyle(nameError).padding = Padding{SpaceL, 0.0f};
            ctx.style(nameError).all.text = DangerColor;
            noticeText(ctx, nameError, renameError);
            if (nameFocused)
                ctx.setFocused(name);
        }

        if (action.bindings.empty())
        {
            const auto line = row(ctx, actionList);
            ctx.layoutStyle(line).preferredSize = Vec2{0.0f, ControlHeight};
            ctx.layoutStyle(line).padding = Padding{SpaceXl + SpaceL, 0.0f, SpaceS, 0.0f};
            (void)mutedLabel(ctx, line, "No bindings. Press + to add one.");
        }

        for (const auto binding : action.bindings)
        {
            const auto line = row(ctx, actionList);
            auto lineLayout = ctx.layoutStyle(line);
            lineLayout.preferredSize = Vec2{0.0f, ControlHeight};
            lineLayout.padding = Padding{SpaceXl + SpaceL, 0.0f, SpaceS, 0.0f};
            lineLayout.gap = SpaceS;

            const auto label = ctx.createLabel(line, bindingLabel(binding));
            ctx.layoutStyle(label).preferredSize = Vec2{196.0f, 0.0f};
            ctx.layoutStyle(label).clip = true;

            std::string overlap;
            for (const auto &other : draft.actions)
            {
                if (other.id == action.id || std::ranges::find(other.bindings, binding) == other.bindings.end())
                    continue;
                overlap += overlap.empty() ? "Also bound to " : ", ";
                overlap += other.name;
            }
            const auto warning = ctx.createLabel(line, overlap);
            ctx.layoutStyle(warning).preferredSize = Vec2{};
            ctx.layoutStyle(warning).clip = true;
            ctx.style(warning).all.text = WarningColor;
            ctx.setTooltip(warning, overlap);

            entry.bindings.push_back({iconButton(ctx, line, "refresh", "Replace binding"),
                                      iconButton(ctx, line, "close", "Remove binding")});
        }
    }

    if (actionRows.empty())
        (void)mutedLabel(ctx, actionList, "No actions match the filter.");
}

void ProjectInputEditor::focusSelected(Context &ctx, bool binding)
{
    const auto entry = std::ranges::find(actionRows, selected, &ActionRow::id);
    if (entry == actionRows.end())
        ctx.setFocused(newName);
    else
        ctx.setFocused(binding || !ctx.isValid(entry->label) ? entry->addBinding : entry->label);
}

bool ProjectInputEditor::dirty(const Context &ctx) const
{
    return draft != saved || renamePending(ctx);
}

void ProjectInputEditor::showNotice(Context &ctx, std::string_view text) const
{
    noticeText(ctx, notice, text);
}

void ProjectInputEditor::setRenameError(Context &ctx, std::string_view text)
{
    renameError = text;
    if (ctx.isValid(nameError))
        noticeText(ctx, nameError, text);
}

bool ProjectInputEditor::cancelNameEdit(Context &ctx)
{
    if (ctx.isFocused(newName) && !ctx.text(newName).empty())
    {
        ctx.setText(newName, "");
        noticeText(ctx, newNameError, {});
        return true;
    }
    if (ctx.isFocused(filter) && !ctx.text(filter).empty())
    {
        ctx.setText(filter, "");
        return true;
    }
    if (!renaming)
        return false;

    endRename(ctx);
    return true;
}

void ProjectInputEditor::startRename(Context &ctx)
{
    renaming = true;
    invalidRename = false;
    setRenameError(ctx, {});
    refresh(ctx);
    if (ctx.isValid(name))
        ctx.setFocused(name);
}

void ProjectInputEditor::endRename(Context &ctx)
{
    renaming = invalidRename = false;
    rejectedName.clear();
    setRenameError(ctx, {});
    refresh(ctx);
    focusSelected(ctx, false);
}

void ProjectInputEditor::commitName(Context &ctx)
{
    const auto index = selectedIndex();
    if (!renaming || !ctx.isValid(name) || index == draft.actions.size())
        return;
    if (ctx.text(name) == draft.actions[index].name)
    {
        endRename(ctx);
        return;
    }

    auto candidate = draft;
    candidate.actions[index].name = ctx.text(name);
    const auto error = normalizeProjectInput(candidate, false);
    invalidRename = !error.empty();
    rejectedName = invalidRename ? std::string(ctx.text(name)) : std::string{};
    setRenameError(ctx, error);
    if (invalidRename)
        return;

    draft = std::move(candidate);
    renaming = false;
    showNotice(ctx, {});
    refresh(ctx);
    focusSelected(ctx, false);
}

std::string ProjectInputEditor::apply(Context &ctx, Scribe::Session &session)
{
    commitName(ctx);
    if (invalidRename)
        return renameError;

    const auto result = session.setProjectInput(draft);
    if (!result.ok())
        return result.error;

    saved = draft;
    showNotice(ctx, "Input changes saved.");
    return {};
}

void ProjectInputEditor::deleteSelected(Context &ctx)
{
    const auto index = selectedIndex();
    if (index == draft.actions.size())
        return;

    draft.actions.erase(draft.actions.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto &assignment : draft.assignments)
        if (assignment == selected)
            assignment = {};
    selected = {};
    renaming = invalidRename = false;
    setRenameError(ctx, {});
    showNotice(ctx, {});
    refresh(ctx);
    focusSelected(ctx, false);
}

void ProjectInputEditor::appendControls(std::vector<Handle> &controls) const
{
    controls.insert(controls.end(), {filter, newName, addAction, revert});
    for (const auto &entry : actionRows)
    {
        controls.push_back(entry.id == selected && renaming ? name : entry.label);
        controls.insert(controls.end(), {entry.addBinding, entry.remove});
        for (const auto &binding : entry.bindings)
            controls.insert(controls.end(), {binding.replace, binding.remove});
    }
    controls.insert(controls.end(), assignments.begin(), assignments.end());
}

void ProjectInputEditor::setEditable(Context &ctx, bool canEdit)
{
    for (auto control : {newName, addAction, name})
        if (ctx.isValid(control))
            ctx.setEnabled(control, canEdit);
    for (const auto &entry : actionRows)
    {
        ctx.setEnabled(entry.addBinding, canEdit);
        ctx.setEnabled(entry.remove, canEdit);
        for (const auto &binding : entry.bindings)
        {
            ctx.setEnabled(binding.replace, canEdit);
            ctx.setEnabled(binding.remove, canEdit);
        }
    }
    for (auto control : assignments)
        ctx.setEnabled(control, canEdit);
    ctx.setEnabled(revert, canEdit && dirty(ctx));
}

void ProjectInputEditor::update(Context &ctx, GLFWwindow *window, EditorDialog &dialog,
                                Handle activated, Handle entered, bool canEdit)
{
    const auto chosen = [&](Handle control) {
        return ctx.isValid(control) && ctx.isEnabled(control) && ctx.tree().isVisibleBranch(control) &&
               (ctx.isClicked(control) || activated == control);
    };
    clickAge = std::min(1.0f, clickAge + ctx.input().deltaTime);

    if (canEdit && renaming && ctx.isValid(name) &&
        (entered == name || (!ctx.isFocused(name) && (!invalidRename || ctx.text(name) != rejectedName))))
        commitName(ctx);
    if (!canEdit && renaming && !invalidRename && !renamePending(ctx))
        endRename(ctx);

    if (ctx.text(filter) != filterText)
    {
        filterText = std::string(ctx.text(filter));
        refresh(ctx);
    }

    for (const auto &entry : actionRows)
    {
        if (!chosen(entry.label))
            continue;
        if (entry.id != selected && (invalidRename || renamePending(ctx)))
            break;

        const auto id = entry.id;
        const bool twice = ctx.isClicked(entry.label) && lastClicked == id && clickAge <= 0.35f;
        const bool rename = canEdit && (twice || entered == entry.label);
        lastClicked = id;
        clickAge = twice ? 1.0f : 0.0f;
        selected = id;
        if (rename)
            startRename(ctx);
        else
        {
            refresh(ctx);
            focusSelected(ctx, false);
        }
        return;
    }

    ctx.setEnabled(revert, canEdit && dirty(ctx));
    if (!canEdit)
        return;

    if (chosen(revert))
    {
        draft = saved;
        renaming = invalidRename = false;
        setRenameError(ctx, {});
        showNotice(ctx, {});
        refresh(ctx);
        return;
    }

    if (chosen(addAction) || entered == newName)
    {
        if (invalidRename)
        {
            noticeText(ctx, newNameError, "Finish renaming the selected action first.");
            return;
        }
        auto candidate = draft;
        const auto id = generateInputActionId();
        candidate.actions.push_back({id, std::string(ctx.text(newName)), {}});
        const auto error = normalizeProjectInput(candidate, false);
        noticeText(ctx, newNameError, error);
        if (error.empty())
        {
            draft = std::move(candidate);
            selected = id;
            renaming = false;
            ctx.setText(newName, "");
            ctx.setText(filter, "");
            filterText.clear();
            showNotice(ctx, {});
            refresh(ctx);
            focusSelected(ctx, true);
        }
        return;
    }

    for (const auto &entry : actionRows)
    {
        if (entry.id != selected && invalidRename)
            continue;

        const auto id = entry.id;
        std::optional<std::size_t> replace, remove;
        for (std::size_t i = 0; i < entry.bindings.size(); ++i)
        {
            if (chosen(entry.bindings[i].replace))
                replace = i;
            else if (chosen(entry.bindings[i].remove))
                remove = i;
        }
        const bool add = chosen(entry.addBinding);
        const bool erase = chosen(entry.remove);
        if (!add && !erase && !replace && !remove)
            continue;

        if (selected != id)
        {
            selected = id;
            renaming = false;
            refresh(ctx);
        }
        const auto index = selectedIndex();
        if (index == draft.actions.size())
            return;

        if (erase)
        {
            std::vector<std::string> affected;
            for (std::size_t i = 0; i < assignments.size(); ++i)
                if (draft.assignments[i] == selected)
                    affected.emplace_back(controlLabels[i]);

            if (affected.empty())
                deleteSelected(ctx);
            else
            {
                dialog.show(ctx, "Delete " + draft.actions[index].name + "?",
                            "These controls will become unassigned.", "Delete", "", "Cancel", ButtonRole::Destructive);
                dialog.setDetails(ctx, affected);
            }
        }
        else if (add || replace)
            startCapture(ctx, window, replace);
        else if (auto &bindings = draft.actions[index].bindings; *remove < bindings.size())
        {
            bindings.erase(bindings.begin() + static_cast<std::ptrdiff_t>(*remove));
            showNotice(ctx, {});
            refresh(ctx);
            focusSelected(ctx, true);
        }
        return;
    }

    for (std::size_t i = 0; i < assignments.size(); ++i)
    {
        const auto item = ctx.comboBoxSelectedIndex(assignments[i]);
        if (item < 0 || static_cast<std::size_t>(item) > draft.actions.size())
            continue;

        const auto *action = item > 0 ? &draft.actions[static_cast<std::size_t>(item - 1)] : nullptr;
        const auto id = action ? action->id : InputActionId{};
        if (draft.assignments[i] == id)
            continue;

        draft.assignments[i] = id;
        showNotice(ctx, {});
        refresh(ctx);
        return;
    }
}

void ProjectInputEditor::showCapture(Context &ctx, std::string_view input, std::string_view message, bool error)
{
    ctx.setText(captureInput, input);
    noticeText(ctx, captureMessage, message);
    ctx.style(captureMessage).all.text = error ? DangerColor : ctx.theme().textMuted;
}

void ProjectInputEditor::startCapture(Context &ctx, GLFWwindow *window, std::optional<std::size_t> index)
{
    if (listener)
        return;

    captureWindow = window;
    listener = this;
    replacing = index;
    captured.reset();
    lostFocus = false;

    previousKey = glfwSetKeyCallback(window, [](GLFWwindow *owner, int key, int, int action, int) {
        if (listener && listener->captureWindow == owner && action == GLFW_PRESS &&
            (!listener->captured || key == GLFW_KEY_ESCAPE))
            listener->captured = InputBinding{InputDevice::Key, key};
    });
    previousMouse = glfwSetMouseButtonCallback(window, [](GLFWwindow *owner, int button, int action, int) {
        if (listener && listener->captureWindow == owner && action == GLFW_PRESS && !listener->captured)
            listener->captured = InputBinding{InputDevice::MouseButton, button};
    });
    previousFocus = glfwSetWindowFocusCallback(window, [](GLFWwindow *owner, int focused) {
        if (!listener || listener->captureWindow != owner)
            return;
        if (!focused)
            listener->lostFocus = true;
        if (listener->previousFocus)
            listener->previousFocus(owner, focused);
    });
    
    const auto *action = draft.find(selected);
    const std::string actionName = action ? action->name : std::string{};
    ctx.setText(captureTitle, (index ? "Replace binding for " : "Add binding for ") + actionName);
    const bool current = action && index && *index < action->bindings.size();
    showCapture(ctx, "Waiting for input...", current ? "Current: " + bindingLabel(action->bindings[*index]) : std::string{}, false);
    showNotice(ctx, {});
    ctx.clearFocus();
    ctx.setVisible(captureBackdrop, true);
}

void ProjectInputEditor::cancelCapture()
{
    if (!captureWindow)
        return;

    glfwSetKeyCallback(captureWindow, previousKey);
    glfwSetMouseButtonCallback(captureWindow, previousMouse);
    glfwSetWindowFocusCallback(captureWindow, previousFocus);
    captureWindow = nullptr;
    listener = nullptr;
    captured.reset();
}

void ProjectInputEditor::stopCapture(Context &ctx)
{
    cancelCapture();
    ctx.setVisible(captureBackdrop, false);
}

void ProjectInputEditor::finishCapture(Context &ctx, bool canCapture)
{
    if (!canCapture || lostFocus || captured == InputBinding{InputDevice::Key, GLFW_KEY_ESCAPE})
    {
        stopCapture(ctx);
        showNotice(ctx, "Binding capture cancelled.");
        focusSelected(ctx, true);
        return;
    }
    if (!captured)
        return;

    const auto binding = *captured;
    captured.reset();
    if (binding == InputBinding{InputDevice::Key, GLFW_KEY_F5})
    {
        showCapture(ctx, bindingLabel(binding), "F5 is reserved for Play and Stop. Press another input.", true);
        return;
    }
    const auto index = selectedIndex();
    if (index == draft.actions.size())
    {
        stopCapture(ctx);
        return;
    }

    auto candidate = draft;
    auto &bindings = candidate.actions[index].bindings;
    if (replacing && *replacing < bindings.size())
        bindings[*replacing] = binding;
    else
        bindings.push_back(binding);

    const auto error = normalizeProjectInput(candidate, false);
    if (!error.empty())
    {
        showCapture(ctx, bindingLabel(binding), error + " Press another input.", true);
        return;
    }

    draft = std::move(candidate);
    stopCapture(ctx);
    showNotice(ctx, {});
    refresh(ctx);
    focusSelected(ctx, true);
}
} // namespace Cthulhu::Editor