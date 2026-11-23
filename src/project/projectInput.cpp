#include "projectInput.hpp"

#include <algorithm>
#include <functional>

#include <GLFW/glfw3.h>
#include <simdjson.h>

#include "entityId.hpp"
#include "log_utils.hpp"

namespace Cthulhu::Project
{
namespace
{
constexpr std::uint64_t defaultActionNamespace = 0x435448554C485549ULL;

bool validBinding(InputBinding binding)
{
    const int key = binding.code;
    if (binding.device == InputDevice::MouseButton)
        return key >= GLFW_MOUSE_BUTTON_1 && key <= GLFW_MOUSE_BUTTON_LAST;
    if (binding.device != InputDevice::Key || key == GLFW_KEY_ESCAPE || key == GLFW_KEY_F5)
        return false;

    return key == GLFW_KEY_SPACE || key == GLFW_KEY_APOSTROPHE ||
           key == GLFW_KEY_SEMICOLON || key == GLFW_KEY_EQUAL || key == GLFW_KEY_GRAVE_ACCENT ||
           (key >= GLFW_KEY_COMMA && key <= GLFW_KEY_9) ||
           (key >= GLFW_KEY_A && key <= GLFW_KEY_RIGHT_BRACKET) ||
           (key >= GLFW_KEY_WORLD_1 && key <= GLFW_KEY_WORLD_2) ||
           (key >= GLFW_KEY_ESCAPE && key <= GLFW_KEY_END) ||
           (key >= GLFW_KEY_CAPS_LOCK && key <= GLFW_KEY_PAUSE) ||
           (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) ||
           (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL) ||
           (key >= GLFW_KEY_LEFT_SHIFT && key <= GLFW_KEY_MENU);
}

char foldAscii(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool validInputName(std::string_view name)
{
    if (name.empty() || !simdjson::validate_utf8(name.data(), name.size()))
        return false;

    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const auto byte = static_cast<unsigned char>(name[i]);
        if (byte < 32 || byte == 127)
            return false;
        // UTF-8 encodes C1 control characters as C2 80 through C2 9F.
        if (byte == 0xC2 && i + 1 < name.size() &&
            static_cast<unsigned char>(name[i + 1]) <= 0x9F)
            return false;
    }

    return true;
}
} // namespace

InputActionId generateInputActionId()
{
    auto id = Scene::generateEntityId();
    while (id.high == defaultActionNamespace)
        id = Scene::generateEntityId();
    return {id.high, id.low};
}

std::string inputActionIdToString(InputActionId id)
{
    return Scene::entityIdToString({id.high, id.low});
}

std::optional<InputActionId> inputActionIdFromString(std::string_view value)
{
    const auto id = Scene::entityIdFromString(value);
    if (!id)
        return std::nullopt;

    return InputActionId{id->high, id->low};
}

const InputAction *ProjectInputMap::find(InputActionId id) const noexcept
{
    const auto action = std::ranges::find(actions, id, &InputAction::id);
    return action == actions.end() ? nullptr : &*action;
}

ProjectInputMap defaultProjectInput()
{
    ProjectInputMap map;
    map.actions.reserve(inputControlCount);

    const auto add = [&](InputControl control, std::uint64_t fixedId, const char *name,
                         InputDevice device, int code) {
        const InputActionId id{defaultActionNamespace, fixedId};
        map.actions.push_back({id, name, {{device, code}}});
        map.assignments[static_cast<std::size_t>(control)] = id;
    };

    using enum InputControl;
    using enum InputDevice;
    add(MoveForward,  1, "Move Forward",  Key, GLFW_KEY_W);
    add(MoveBackward, 2, "Move Backward", Key, GLFW_KEY_S);
    add(MoveLeft,     3, "Move Left",     Key, GLFW_KEY_A);
    add(MoveRight,    4, "Move Right",    Key, GLFW_KEY_D);
    add(Jump,         5, "Jump",          Key, GLFW_KEY_SPACE);
    add(Fire,         6, "Fire",          MouseButton, GLFW_MOUSE_BUTTON_LEFT);
    add(Reload,       7, "Reload",        Key, GLFW_KEY_R);
    add(Interact,     8, "Interact",      Key, GLFW_KEY_E);
    add(Sprint,       9, "Sprint",        Key, GLFW_KEY_LEFT_SHIFT);
    add(Crouch,      10, "Crouch",        Key, GLFW_KEY_LEFT_CONTROL);
    return map;
}

std::string normalizeProjectInput(ProjectInputMap &map, bool reportMissingAssignments)
{
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    for (auto action = map.actions.begin(); action != map.actions.end(); ++action)
    {
        if (!action->id.isValid())
            return "An input action has an invalid ID.";

        const auto start = action->name.find_first_not_of(whitespace);
        if (start == std::string::npos)
            return "An input action has a blank name.";
        action->name.erase(action->name.find_last_not_of(whitespace) + 1);
        action->name.erase(0, start);
        if (!validInputName(action->name))
            return "Invalid name on input action " + inputActionIdToString(action->id) + ".";

        for (auto other = map.actions.begin(); other != action; ++other)
        {
            if (other->id == action->id)
                return "Duplicate input action ID " + inputActionIdToString(action->id) + ".";
            if (std::ranges::equal(other->name, action->name, std::equal_to<>{}, foldAscii, foldAscii))
                return "Duplicate input action name '" + action->name + "'.";
        }

        for (auto binding = action->bindings.begin(); binding != action->bindings.end(); ++binding)
        {
            if (!validBinding(*binding))
                return "Unsupported or reserved binding on input action '" + action->name + "'.";
            if (std::find(action->bindings.begin(), binding, *binding) != binding)
                return "Duplicate binding on input action '" + action->name + "'.";
        }
    }

    for (std::size_t i = 0; i < map.assignments.size(); ++i)
    {
        auto &id = map.assignments[i];
        if (!id.isValid() || map.find(id))
            continue;

        if (reportMissingAssignments)
        {
            using namespace KalaHeaders::KalaLog;
            Log::Print("INPUT CONTROL '" + std::string(inputControlKeys[i]) +
                           "' REFERENCES MISSING ACTION " + inputActionIdToString(id) + ". LOADED AS UNASSIGNED.",
                       "ProjectInput", LogType::LOG_WARNING);
        }

        id = {};
    }

    return {};
}

InputActionState evaluateInputAction(const InputAction &action, std::span<const InputBinding> previouslyHeld, std::span<const InputBinding> held) noexcept
{
    const auto isDown = [&](std::span<const InputBinding> buttons) {
        return std::ranges::any_of(action.bindings, [buttons](InputBinding binding) {
            return std::ranges::find(buttons, binding) != buttons.end();
        });
    };

    const bool wasDown = isDown(previouslyHeld);
    const bool down = isDown(held);
    return {down, down && !wasDown, wasDown && !down};
}
} // namespace Cthulhu::Project