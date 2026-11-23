#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Cthulhu::Project
{
struct InputActionId
{
    std::uint64_t high{};
    std::uint64_t low{};

    [[nodiscard]] constexpr bool isValid() const noexcept { return high != 0 || low != 0; }
    friend constexpr bool operator==(const InputActionId &, const InputActionId &) = default;
};

enum class InputDevice : std::uint8_t { Key, MouseButton };

struct InputBinding
{
    InputDevice device{};
    int code{};

    friend constexpr bool operator==(const InputBinding &, const InputBinding &) = default;
};

struct InputAction
{
    InputActionId id;
    std::string name;
    std::vector<InputBinding> bindings;
};

enum class InputControl : std::size_t
{
    MoveForward, MoveBackward, MoveLeft, MoveRight, Jump, Fire, Reload, Interact, Sprint, Crouch,
    Count
};

inline constexpr std::uint32_t inputFormatVersion = 1;
inline constexpr std::size_t inputControlCount = static_cast<std::size_t>(InputControl::Count);
inline constexpr std::array<std::string_view, inputControlCount> inputControlKeys{
    "move_forward", "move_backward", "move_left", "move_right", "jump",
    "fire", "reload", "interact", "sprint", "crouch"
};

struct ProjectInputMap
{
    std::vector<InputAction> actions;
    std::array<InputActionId, inputControlCount> assignments{};

    [[nodiscard]] const InputAction *find(InputActionId id) const noexcept;
};

struct InputActionState
{
    bool down{};
    bool pressed{};
    bool released{};
};

[[nodiscard]] InputActionId generateInputActionId();
[[nodiscard]] std::string inputActionIdToString(InputActionId id);
[[nodiscard]] std::optional<InputActionId> inputActionIdFromString(std::string_view value);
[[nodiscard]] ProjectInputMap defaultProjectInput();
[[nodiscard]] std::string normalizeProjectInput(ProjectInputMap &map, bool reportMissingAssignments);
[[nodiscard]] InputActionState evaluateInputAction(const InputAction &action,
    std::span<const InputBinding> previouslyHeld, std::span<const InputBinding> held) noexcept;
} // namespace Cthulhu::Project