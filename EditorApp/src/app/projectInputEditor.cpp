#include "projectInputEditor.hpp"
#include "editorMetrics.hpp"

#include <algorithm>

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
} // namespace

void ProjectInputEditor::build(Context &ctx, NodeHandle page, const ProjectInputMap &input)
{
    saved = draft = input;
    invalidRename = false;
    rejectedName.clear();
    selected = {};
    bindingRows.clear();
    bindingList = {};
    const auto nav = row(ctx, page);
    actionsTab = ctx.createButton(nav, "Actions");
    controlsTab = ctx.createButton(nav, "Built-in Controls");
    revert = ctx.createButton(nav, "Revert Changes");
    notice = ctx.createLabel(page, "");
    ctx.layoutStyle(notice).clip = true;

    actionView = column(ctx, page);
    const auto add = row(ctx, actionView);
    newName = ctx.createTextInput(add, "", "New action name");
    addAction = ctx.createButton(add, "Add Action");
    newNameError = ctx.createLabel(actionView, "");
    ctx.layoutStyle(newNameError).clip = true;
    selector = ctx.createComboBox(actionView, {});
    details = column(ctx, actionView);
    name = ctx.createTextInput(details, "", "Action name");
    nameError = ctx.createLabel(details, "");
    ctx.layoutStyle(nameError).clip = true;
    const auto buttons = row(ctx, details);
    addBinding = ctx.createButton(buttons, "Add Binding");
    deleteAction = ctx.createButton(buttons, "Delete Action");

    controlView = column(ctx, page);
    for (std::size_t i = 0; i < assignments.size(); ++i)
    {
        const auto line = row(ctx, controlView);
        const auto label = ctx.createLabel(line, controlLabels[i]);
        ctx.layoutStyle(label).preferredSize = Vec2{108.0f, 0.0f};
        assignments[i] = ctx.createComboBox(line, {});
        noBindings[i] = ctx.createLabel(line, "No bindings");
    }

    noticeText(ctx, newNameError, {});
    noticeText(ctx, nameError, {});
    showNotice(ctx, {});
    selectView(ctx, false);
    refresh(ctx);
}

std::size_t ProjectInputEditor::selectedIndex() const
{
    const auto iter = std::ranges::find(draft.actions, selected, &InputAction::id);
    return static_cast<std::size_t>(iter - draft.actions.begin());
}

void ProjectInputEditor::refresh(Context &ctx)
{
    auto index = selectedIndex();
    if (index == draft.actions.size() && !draft.actions.empty())
    {
        index = 0;
        selected = draft.actions[0].id;
    }

    std::vector<std::string> names;
    std::vector<std::string> choices{"Unassigned"};
    for (const auto &action : draft.actions)
    {
        names.push_back(action.name);
        choices.push_back("  " + action.name);
    }
    ctx.setComboBoxItems(selector, names);
    ctx.setComboBoxSelectedIndex(selector, names.empty() ? -1 : static_cast<i32>(index));
    ctx.setEnabled(selector, !names.empty());

    for (std::size_t i = 0; i < assignments.size(); ++i)
    {
        const auto action = std::ranges::find(draft.actions, draft.assignments[i], &InputAction::id);
        const bool assigned = action != draft.actions.end();
        ctx.setComboBoxItems(assignments[i], choices);
        ctx.setComboBoxSelectedIndex(assignments[i], assigned ? static_cast<i32>(action - draft.actions.begin() + 1) : 0);
        ctx.setVisible(noBindings[i], assigned && action->bindings.empty());
    }

    if (ctx.isValid(bindingList))
        ctx.destroyNode(bindingList);
    bindingRows.clear();
    bindingList = column(ctx, details);

    const auto *action = draft.find(selected);
    ctx.setVisible(details, action != nullptr);
    if (!invalidRename)
        ctx.setText(name, action ? std::string_view(action->name) : std::string_view{});
    if (!action)
        return;

    if (action->bindings.empty())
        (void)ctx.createLabel(bindingList, "No bindings");

    for (const auto binding : action->bindings)
    {
        const auto line = row(ctx, bindingList);
        const auto label = ctx.createLabel(line, bindingLabel(binding));
        ctx.layoutStyle(label).clip = true;
        bindingRows.push_back({ctx.createButton(line, "Replace"), ctx.createButton(line, "Remove")});

        std::string overlap;
        for (const auto &other : draft.actions)
        {
            if (other.id == selected || std::ranges::find(other.bindings, binding) == other.bindings.end())
                continue;
            overlap += overlap.empty() ? "Also bound to " : ", ";
            overlap += other.name;
        }
        if (!overlap.empty())
        {
            const auto warning = ctx.createLabel(bindingList, overlap);
            ctx.layoutStyle(warning).clip = true;
            ctx.setTooltip(warning, overlap);
        }
    }
}

void ProjectInputEditor::selectView(Context &ctx, bool controls)
{
    ctx.setVisible(actionView, !controls);
    ctx.setVisible(controlView, controls);
    ctx.style(actionsTab).normal.background = controls ? ctx.theme().surface : ctx.theme().surfaceActive;
    ctx.style(controlsTab).normal.background = controls ? ctx.theme().surfaceActive : ctx.theme().surface;
}

bool ProjectInputEditor::dirty(const Context &ctx) const
{
    const auto *action = draft.find(selected);
    return draft != saved || (action && ctx.text(name) != action->name);
}

void ProjectInputEditor::showNotice(Context &ctx, std::string_view text) const
{
    noticeText(ctx, notice, text);
}

bool ProjectInputEditor::cancelNameEdit(Context &ctx)
{
    if (ctx.isFocused(newName) && !ctx.text(newName).empty())
    {
        ctx.setText(newName, "");
        noticeText(ctx, newNameError, {});
        return true;
    }

    const auto *action = draft.find(selected);
    if (!action || !ctx.isFocused(name) || ctx.text(name) == action->name)
        return false;

    ctx.setText(name, action->name);
    invalidRename = false;
    noticeText(ctx, nameError, {});
    return true;
}

void ProjectInputEditor::commitName(Context &ctx)
{
    const auto index = selectedIndex();
    if (index == draft.actions.size() || ctx.text(name) == draft.actions[index].name)
    {
        invalidRename = false;
        noticeText(ctx, nameError, {});
        return;
    }

    auto candidate = draft;
    candidate.actions[index].name = ctx.text(name);
    const auto error = normalizeProjectInput(candidate, false);
    invalidRename = !error.empty();
    rejectedName = invalidRename ? std::string(ctx.text(name)) : std::string{};
    noticeText(ctx, nameError, error);
    if (invalidRename)
        return;

    draft = std::move(candidate);
    showNotice(ctx, {});
    refresh(ctx);
}

std::string ProjectInputEditor::apply(Context &ctx, Scribe::Session &session)
{
    commitName(ctx);
    if (invalidRename)
        return std::string(ctx.text(nameError));

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
    invalidRename = false;
    noticeText(ctx, nameError, {});
    showNotice(ctx, {});
    refresh(ctx);
    ctx.setFocused(draft.actions.empty() ? newName : selector);
}

void ProjectInputEditor::appendControls(std::vector<Handle> &controls) const
{
    controls.insert(controls.end(), {actionsTab, controlsTab, revert, newName, addAction,
                                     selector, name, addBinding, deleteAction});
    for (const auto &binding : bindingRows)
        controls.insert(controls.end(), {binding.replace, binding.remove});
    controls.insert(controls.end(), assignments.begin(), assignments.end());
}

void ProjectInputEditor::setEditable(Context &ctx, bool canEdit)
{
    for (auto control : {newName, addAction, name, addBinding, deleteAction})
        ctx.setEnabled(control, canEdit);
    for (const auto &binding : bindingRows)
    {
        ctx.setEnabled(binding.replace, canEdit);
        ctx.setEnabled(binding.remove, canEdit);
    }
    for (auto control : assignments)
        ctx.setEnabled(control, canEdit);
    ctx.setEnabled(revert, canEdit && dirty(ctx));
}

void ProjectInputEditor::update(Context &ctx, GLFWwindow *window, EditorDialog &dialog,
                                Handle activated, Handle entered, bool canEdit)
{
    const auto chosen = [&](Handle control) {
        return ctx.isEnabled(control) && ctx.tree().isVisibleBranch(control) &&
               (ctx.isClicked(control) || activated == control);
    };

    if (chosen(actionsTab) || chosen(controlsTab))
        selectView(ctx, chosen(controlsTab));

    const auto *current = draft.find(selected);
    if (canEdit && current && (invalidRename || ctx.text(name) != current->name) &&
        (entered == name || (!ctx.isFocused(name) && (!invalidRename || ctx.text(name) != rejectedName))))
        commitName(ctx);

    auto index = selectedIndex();
    const auto choice = ctx.comboBoxSelectedIndex(selector);
    if (choice >= 0 && static_cast<std::size_t>(choice) < draft.actions.size() &&
        static_cast<std::size_t>(choice) != index)
    {
        if (invalidRename || (index < draft.actions.size() && ctx.text(name) != draft.actions[index].name))
            ctx.setComboBoxSelectedIndex(selector, static_cast<i32>(index));
        else
        {
            index = static_cast<std::size_t>(choice);
            selected = draft.actions[index].id;
            refresh(ctx);
        }
    }

    ctx.setEnabled(revert, canEdit && dirty(ctx));
    if (!canEdit)
        return;

    if (chosen(revert))
    {
        draft = saved;
        invalidRename = false;
        noticeText(ctx, nameError, {});
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
            ctx.setText(newName, "");
            showNotice(ctx, {});
            refresh(ctx);
            ctx.setFocused(addBinding);
        }
        return;
    }

    if (index < draft.actions.size() && chosen(deleteAction))
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
                        "These controls will become unassigned.", "Delete", "");
            dialog.setDetails(ctx, affected);
        }
        return;
    }

    if (index < draft.actions.size() && chosen(addBinding))
    {
        startCapture(ctx, window, std::nullopt);
        return;
    }
    for (std::size_t i = 0; i < bindingRows.size(); ++i)
    {
        if (chosen(bindingRows[i].replace))
        {
            startCapture(ctx, window, i);
            return;
        }
        if (chosen(bindingRows[i].remove) && index < draft.actions.size())
        {
            auto &bindings = draft.actions[index].bindings;
            bindings.erase(bindings.begin() + static_cast<std::ptrdiff_t>(i));
            showNotice(ctx, {});
            refresh(ctx);
            ctx.setFocused(addBinding);
            return;
        }
    }

    if (ctx.isVisible(controlView))
    {
        for (std::size_t i = 0; i < assignments.size(); ++i)
        {
            const auto item = ctx.comboBoxSelectedIndex(assignments[i]);
            if (item < 0 || static_cast<std::size_t>(item) > draft.actions.size())
                continue;

            const auto *action = item > 0 ? &draft.actions[static_cast<std::size_t>(item - 1)] : nullptr;
            const auto id = action ? action->id : InputActionId{};
            if (draft.assignments[i] != id)
                showNotice(ctx, {});
            draft.assignments[i] = id;
            ctx.setVisible(noBindings[i], action && action->bindings.empty());
        }
    }
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
    showNotice(ctx, "Press a key or mouse button. Escape cancels.");
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

void ProjectInputEditor::finishCapture(Context &ctx, bool canCapture)
{
    if (!canCapture || lostFocus || captured == InputBinding{InputDevice::Key, GLFW_KEY_ESCAPE})
    {
        showNotice(ctx, "Binding capture cancelled.");
        cancelCapture();
        return;
    }
    if (!captured)
        return;

    const auto binding = *captured;
    captured.reset();
    if (binding == InputBinding{InputDevice::Key, GLFW_KEY_F5})
    {
        showNotice(ctx, "F5 is reserved for Play and Stop. Press another input or Escape.");
        return;
    }
    const auto index = selectedIndex();
    if (index == draft.actions.size())
    {
        cancelCapture();
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
        showNotice(ctx, error + " Press another input or Escape.");
        return;
    }

    draft = std::move(candidate);
    cancelCapture();
    showNotice(ctx, {});
    refresh(ctx);
    ctx.setFocused(addBinding);
}
} // namespace Cthulhu::Editor