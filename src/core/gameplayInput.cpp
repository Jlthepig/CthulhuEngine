#include <algorithm>
#include <cstdint>

#include "gameplayInput.hpp"
#include "input.hpp"

namespace Cthulhu::Core
{
namespace
{
std::uint16_t addCount(std::uint16_t current, std::uint16_t added) noexcept
{
    const std::uint32_t sum = static_cast<std::uint32_t>(current) + added;
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(sum, UINT16_MAX));
}

const GameplayControlState neutralControl{};
} // namespace

void GameplayInput::setMap(const Project::ProjectInputMap &map)
{
    inputMap = map;
    actions.assign(inputMap.actions.size(), ActionState{});
    controlActions.fill(SIZE_MAX);

    for (std::size_t control = 0; control < Project::inputControlCount; ++control)
    {
        const auto id = inputMap.assignments[control];
        const auto found = std::ranges::find(inputMap.actions, id, &Project::InputAction::id);

        if (id.isValid() && found != inputMap.actions.end())
            controlActions[control] = static_cast<std::size_t>(found - inputMap.actions.begin());
    }

    Input::clearButtonEdges();
}

void GameplayInput::gather()
{
    for (std::size_t i = 0; i < actions.size(); ++i)
    {
        ActionState &state = actions[i];
        state.held = false;

        for (const Project::InputBinding binding : inputMap.actions[i].bindings)
        {
            const bool key = binding.device == Project::InputDevice::Key;
            const Input::ButtonEdges edges = key ? Input::getKeyEdges(binding.code) : Input::getMouseButtonEdges(binding.code);

            state.pendingPresses = addCount(state.pendingPresses, edges.presses);
            state.pendingReleases = addCount(state.pendingReleases, edges.releases);
            state.held = state.held || (key ? Input::isKeyDown(binding.code) : Input::isMouseButtonDown(binding.code));
        }
    }

    // Cleared after all actions read them, so actions sharing a binding all see the press.
    Input::clearButtonEdges();
}

void GameplayInput::discard()
{
    for (ActionState &state : actions)
        state = {};

    Input::clearButtonEdges();
}

void GameplayInput::beginStep()
{
    for (ActionState &state : actions)
    {
        // A tap that went down and up within one frame still shows as down for this step.
        state.step = {state.held || state.pendingPresses > 0, state.pendingPresses, state.pendingReleases};
        state.pendingPresses = 0;
        state.pendingReleases = 0;
    }
}

const GameplayControlState &GameplayInput::control(Project::InputControl control) const noexcept
{
    const auto index = static_cast<std::size_t>(control);

    if (index >= controlActions.size() || controlActions[index] >= actions.size())
        return neutralControl;

    return actions[controlActions[index]].step;
}
} // namespace Cthulhu::Core