#pragma once

#include <array>
#include <fstream>

#include "../testCommon.hpp"
#include "project.hpp"

#include <GLFW/glfw3.h>

namespace Cthulhu::Validation
{
inline void validateProjectInput(const std::filesystem::path &directory, Results &r)
{
    using namespace Cthulhu::Project;
    using ProjectFile = Cthulhu::Project::Project;
    const auto root = directory / ("validation_input_" + inputActionIdToString(generateInputActionId()));
    auto project = ProjectFile::createProject(root, {});
    check(r, project.has_value(), "Input: create and reopen a project with saved defaults");
    if (!project)
        return;

    struct Cleanup
    {
        const std::filesystem::path &root;
        ~Cleanup()
        {
            std::error_code error;
            std::filesystem::remove_all(root, error);
        }
    } cleanup{root};

    const auto &file = project->getProjectFilePath();
    const auto &defaults = project->getConfig().input;
    const auto defaultIds = defaults.assignments;
    constexpr auto fire = static_cast<std::size_t>(InputControl::Fire);
    constexpr auto forward = static_cast<std::size_t>(InputControl::MoveForward);
    constexpr std::array expected{
        InputBinding{InputDevice::Key, GLFW_KEY_W}, InputBinding{InputDevice::Key, GLFW_KEY_S},
        InputBinding{InputDevice::Key, GLFW_KEY_A}, InputBinding{InputDevice::Key, GLFW_KEY_D},
        InputBinding{InputDevice::Key, GLFW_KEY_SPACE}, InputBinding{InputDevice::MouseButton, GLFW_MOUSE_BUTTON_LEFT},
        InputBinding{InputDevice::Key, GLFW_KEY_R}, InputBinding{InputDevice::Key, GLFW_KEY_E},
        InputBinding{InputDevice::Key, GLFW_KEY_LEFT_SHIFT}, InputBinding{InputDevice::Key, GLFW_KEY_LEFT_CONTROL}
    };

    bool allDefaults = defaults.actions.size() == expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const auto *action = defaults.find(defaultIds[i]);
        const std::span held{&expected[i], 1};
        allDefaults &= action && evaluateInputAction(*action, {}, held).pressed &&
                       evaluateInputAction(*action, held, {}).released;
    }
    check(r, allDefaults, "Input: all saved keyboard and mouse defaults resolve and produce edges");
    if (!allDefaults)
        return;

    auto edited = defaults;
    const auto backupBinding = InputBinding{InputDevice::Key, GLFW_KEY_UP};
    edited.actions[forward].bindings.push_back(backupBinding);
    edited.actions[fire].name = "  Tir \xC3\x89" " #1 \"Main\"  ";
    const bool saved = project->setInputMap(edited).empty() &&
                       project->setDetails("Input Validation", 960, 540).empty();
    const auto reopened = saved ? ProjectFile::open(file) : std::nullopt;
    const auto *renamed = reopened ? reopened->getConfig().input.find(defaultIds[fire]) : nullptr;
    check(r, renamed && renamed->name == "Tir \xC3\x89" " #1 \"Main\"" &&
                 reopened->getConfig().input.assignments == defaultIds,
          "Input: rename and an unrelated settings save preserve action IDs and all assignments");

    const auto *move = reopened ? reopened->getConfig().input.find(defaultIds[forward]) : nullptr;
    const std::span primary{&expected[forward], 1};
    const std::span alternative{&backupBinding, 1};
    const auto switched = move ? evaluateInputAction(*move, primary, alternative) : InputActionState{};
    check(r, move && switched.down && !switched.pressed && !switched.released &&
                 evaluateInputAction(*move, alternative, {}).released,
          "Input: alternative bindings share one down state and one release");

    edited.actions[fire].name = " move forward ";
    check(r, !project->setInputMap(edited).empty() &&
                 project->getConfig().input.find(defaultIds[fire])->name == "Tir \xC3\x89" " #1 \"Main\"",
          "Input: duplicate names fail without changing the live map");

    edited.actions.erase(edited.actions.begin() + fire);
    const auto deleted = project->setInputMap(edited).empty() ? ProjectFile::open(file) : std::nullopt;
    check(r, deleted && !deleted->getConfig().input.assignments[fire].isValid() &&
                 !deleted->getConfig().input.find(defaultIds[fire]),
          "Input: deleting an assigned action clears its saved assignment");
    const auto empty = project->setInputMap({}).empty() ? ProjectFile::open(file) : std::nullopt;
    check(r, empty && empty->getConfig().input.actions.empty() &&
                 !empty->getConfig().input.assignments[fire].isValid(),
          "Input: explicitly empty maps remain empty after reopening");

    const std::string legacy = "name = \"Legacy\"\nwindowWidth = 960\nwindowHeight = 540\n";
    const auto writeFixture = [&](std::string_view contents) {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << contents;
        out.close();
        return static_cast<bool>(out);
    };

    const auto migrated = writeFixture(legacy) ? ProjectFile::open(file) : std::nullopt;
    const auto reread = migrated ? ProjectFile::open(file) : std::nullopt;
    check(r, reread && migrated->getConfig().input.assignments == defaultIds &&
                 reread->getConfig().input.assignments == defaultIds,
          "Input: legacy projects get stable default IDs on repeated opens");

    std::string dangling = legacy + "input.version = 1\n";
    for (std::size_t i = 0; i < inputControlCount; ++i)
    {
        dangling += "input.control." + std::string(inputControlKeys[i]) + " = " +
                    (i == fire ? inputActionIdToString(defaultIds[fire]) : "none") + "\n";
    }
    const auto recovered = writeFixture(dangling) ? ProjectFile::open(file) : std::nullopt;
    check(r, recovered && !recovered->getConfig().input.assignments[fire].isValid(),
          "Input: a dangling assignment loads as unassigned");
    check(r, writeFixture(legacy + "input.version = 1\n") && !ProjectFile::open(file),
          "Input: incomplete input section fails to open");
    check(r, writeFixture(legacy + "input.version = 999\n") && !ProjectFile::open(file),
          "Input: an unsupported input version fails to open");
}
} // namespace Cthulhu::Validation