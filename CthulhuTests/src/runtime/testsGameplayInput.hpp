#pragma once

#include "../testCommon.hpp"
#include "gameplayInput.hpp"
#include "input.hpp"

#include <GLFW/glfw3.h>

namespace Cthulhu::Validation
{
inline void validateGameplayInput(Engine &engine, Results &r)
{
    using Core::GameplayInput;
    using Core::Input;
    using Project::InputControl;

    const auto tapKey = [](int key, int times) {
        for (int i = 0; i < times; ++i)
        {
            Input::key_callback(nullptr, key, 0, GLFW_PRESS, 0);
            Input::key_callback(nullptr, key, 0, GLFW_RELEASE, 0);
        }
    };

    GameplayInput input;
    input.setMap(Project::defaultProjectInput());
    const auto jump = [&input]() { return input.control(InputControl::Jump); };
    const auto fire = [&input]() { return input.control(InputControl::Fire); };

    tapKey(GLFW_KEY_SPACE, 1);
    input.gather(); // frame with no fixed step
    input.gather(); // next frame
    input.beginStep();
    check(r, jump().pressCount == 1 && jump().releaseCount == 1 && jump().down,
          "GI1 a tap survives a frame with no fixed steps");

    tapKey(GLFW_KEY_SPACE, 1);
    input.gather();
    int delivered = 0;
    for (int step = 0; step < 3; ++step)
    {
        input.beginStep();
        delivered += jump().pressCount;
    }
    check(r, delivered == 1, "GI2 a frame with several steps delivers a press once");

    tapKey(GLFW_KEY_SPACE, 3);
    input.gather();
    input.beginStep();
    check(r, jump().pressCount == 3 && jump().releaseCount == 3,
          "GI3 several taps in one frame are counted on the next step");

    tapKey(GLFW_KEY_SPACE, 1);
    input.discard();
    input.gather();
    input.beginStep();
    check(r, jump().pressCount == 0 && !jump().down, "GI4 releasing capture discards buffered presses");

    Input::mouse_button_callback(nullptr, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    Input::suppressMouseButtonUntilRelease(GLFW_MOUSE_BUTTON_LEFT);
    Input::mouse_button_callback(nullptr, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
    input.gather();
    input.beginStep();
    check(r, fire().pressCount == 0 && fire().releaseCount == 0, "GI5 the capture click never reaches gameplay");

    Input::mouse_button_callback(nullptr, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
    input.gather();
    input.beginStep();
    check(r, fire().pressCount == 1, "GI6 the next click after capture fires");
    Input::mouse_button_callback(nullptr, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);

    auto shared = Project::defaultProjectInput();
    const auto interactId = shared.assignments[static_cast<std::size_t>(InputControl::Interact)];
    for (auto &action : shared.actions)
    {
        if (action.id == interactId)
            action.bindings = {{Project::InputDevice::Key, GLFW_KEY_SPACE}};
    }
    input.setMap(shared);
    tapKey(GLFW_KEY_SPACE, 1);
    input.gather();
    input.beginStep();
    check(r, jump().pressCount == 1 && input.control(InputControl::Interact).pressCount == 1,
          "GI7 actions sharing a binding both receive the press");

    check(r, engine.getGameplayInput().map() == engine.getProject()->getConfig().input,
          "GI8 gameplay uses the saved project map");

    const auto previousMode = engine.getWorldMode();
    const auto previousSimulation = engine.getSimulationState();

    if (!engine.createEmptyScene("V_GameplayInput"))
    {
        check(r, false, "GI9 gameplay input scene setup");
        return;
    }

    engine.setWorldMode(WorldMode::Play);
    engine.setSimulationState(SimulationState::Paused);
    tapKey(GLFW_KEY_SPACE, 1);

    if (engine.prepareFrame())
    {
        engine.stepSimulation();
        engine.advanceSimulation(true);
    }

    const auto &engineJump = engine.getGameplayInput().control(InputControl::Jump);
    check(r, engineJump.pressCount == 0 && !engineJump.down && engine.getSimulationState() == SimulationState::Paused,
          "GI9 a paused Step runs with neutral input");

    Input::clearButtonEdges();
    engine.setWorldMode(previousMode);
    engine.setSimulationState(previousSimulation);
    check(r, engine.loadScene(SAVE_PATH), "GI10 restore validation scene");
}
} // namespace Cthulhu::Validation