#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "projectInput.hpp"

namespace Cthulhu::Core
{
struct GameplayControlState
{
    bool down{};
    std::uint16_t pressCount{};
    std::uint16_t releaseCount{};
};

// Maps raw button edges to project actions for the fixed tick.
// Edges gathered across render frames are delivered together to the next fixed step.
class GameplayInput
{
  public:
    void setMap(const Project::ProjectInputMap &map);
    void gather();
    void discard();
    void beginStep();

    [[nodiscard]] const GameplayControlState &control(Project::InputControl control) const noexcept;

    [[nodiscard]] std::size_t actionCount() const noexcept
    {
        return actions.size();
    }

    [[nodiscard]] const GameplayControlState &actionState(std::size_t index) const noexcept
    {
        return actions[index].step;
    }

    [[nodiscard]] const Project::ProjectInputMap &map() const noexcept
    {
        return inputMap;
    }

  private:
    struct ActionState
    {
        bool held{};
        std::uint16_t pendingPresses{};
        std::uint16_t pendingReleases{};
        GameplayControlState step{};
    };

    Project::ProjectInputMap inputMap;
    std::vector<ActionState> actions;
    std::array<std::size_t, Project::inputControlCount> controlActions{};
};
} // namespace Cthulhu::Core