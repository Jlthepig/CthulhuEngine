#pragma once

#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

#include "systemRegistry.hpp"
#include "../testCommon.hpp"
#include "input.hpp"

namespace Cthulhu::Validation
{
inline constexpr const char* GAMEPLAY_SYSTEMS[] = {"PhysicsSyncSystem", "CharacterInterpolationSystem", "WeaponSystem",
												   "AudioSystem"};

inline bool gameplaySystemsAre(Engine& engine, bool enabled)
{
	auto* scene = engine.getActiveScene();
	if (!scene)
	{
		return false;
	}

	for (const char* name : GAMEPLAY_SYSTEMS)
	{
		auto system = scene->getWorld().lookup(name);
		if (!system || system.has(flecs::Disabled) == enabled)
		{
			return false;
		}
	}
	return true;
}

inline void validateInputRouting(Engine &engine, Results &r)
{
    using Core::Input;

    const auto previousMode = engine.getWorldMode();
    const auto previousSimulation = engine.getSimulationState();

    if (!engine.createEmptyScene("V_InputRouting"))
    {
        check(r, false, "IR0 input scene setup");
        return;
    }

    engine.setWorldMode(WorldMode::Play);

    int ticks = 0;
    float observedX = 0.0f;
    float observedY = 0.0f;
    float observedScroll = 0.0f;

    auto probe = engine.getActiveScene()->getWorld()
        .system("V_InputProbe")
        .kind(flecs::OnUpdate)
        .run([&](flecs::iter &) {
            ++ticks;
            observedX = Input::getMouseDeltaX();
            observedY = Input::getMouseDeltaY();
            observedScroll = Input::getScrollDeltaY();
        });

    probe.add<CS::GameplaySystem>();
    engine.setSimulationState(SimulationState::Running);

    const auto injectMotion = []() {
        Input::resetMouseTracking();
        Input::mouse_callback(nullptr, 0.0, 0.0);
        (void)Input::getMouseDeltaX();
        (void)Input::getMouseDeltaY();
        (void)Input::getScrollDeltaY();

        Input::mouse_callback(nullptr, 5.0, -7.0);
        Input::scroll_callback(nullptr, 0.0, 3.0);
    };

    for (const bool acceptInput : {false, true})
    {
        const int before = ticks;
        const bool prepared = engine.prepareFrame();
        check(r, prepared && ticks == before,
              "IR1 preparation does not advance gameplay");

        if (!prepared)
            break;

        injectMotion();
        engine.advanceSimulation(acceptInput);

        check(r, ticks == before + 1,
              "IR2 simulation advances once with either input state");

        check(r,
              observedX == (acceptInput ? 5.0f : 0.0f) &&
              observedY == (acceptInput ? 7.0f : 0.0f) &&
              observedScroll == (acceptInput ? 3.0f : 0.0f),
              "IR3 gameplay receives motion only when allowed");

        check(r, Input::getMouseDeltaX() == 0.0f &&
                     Input::getMouseDeltaY() == 0.0f &&
                     Input::getScrollDeltaY() == 0.0f,
              "IR4 gameplay leaves no accumulated motion");

        injectMotion();
        check(r, Input::getMouseDeltaX() == 5.0f &&
                     Input::getMouseDeltaY() == 7.0f &&
                     Input::getScrollDeltaY() == 3.0f,
              "IR5 raw input is restored outside simulation");
    }

    engine.setSimulationState(SimulationState::Paused);
    const int beforePause = ticks;
    if (engine.prepareFrame())
        engine.advanceSimulation(false);

    check(r, ticks == beforePause,
          "IR6 paused gameplay remains paused");

    // The system captures stack variables; destroy it before they expire.
    probe.destruct();
    Input::resetMouseTracking();
    (void)Input::getScrollDeltaY();

    engine.setWorldMode(previousMode);
    engine.setSimulationState(previousSimulation);
    check(r, engine.loadScene(SAVE_PATH),
          "IR7 restore validation scene");
}

inline void validateGameCamera(Engine &engine, Results &r)
{
    CS::Scene scene(engine.getComponentRegistry());
    CS::Camera view = *engine.getCamera();
    using Status = CS::GameCameraStatus;

    check(r, view.updateFromScene(scene) == Status::Missing,
          "GC1 empty scene has no game camera");

    auto first = scene.createEntity("V_FirstCamera");
    CS::CameraComponent settings;
    settings.front = glm::vec3(0.0f, 0.0f, -4.0f);
    settings.fov = 65.0f;
    first.set(settings);

    auto transform = first.get<CS::TransformComponent>();
    transform.cachedModelMatrix[3] = glm::vec4(4.0f, 5.0f, 6.0f, 1.0f);
    first.set(transform);

    check(r, view.updateFromScene(scene) == Status::Ready &&
                 view.getPosition() == glm::vec3(4.0f, 5.0f, 6.0f) &&
                 view.getFront() == glm::vec3(0.0f, 0.0f, -1.0f) &&
                 view.getFov() == 65.0f,
          "GC2 sole camera uses world position, normalized direction and FOV");

    auto second = scene.createEntity("V_SecondCamera");
    second.set(CS::CameraComponent{});

    check(r, view.updateFromScene(scene) == Status::NoPrimary,
          "GC3 multiple cameras need a primary");

    settings.primary = true;
    first.set(settings);
    check(r, view.updateFromScene(scene) == Status::Ready &&
                 view.getFov() == 65.0f,
          "GC4 primary camera wins");

    auto secondSettings = second.get<CS::CameraComponent>();
    secondSettings.primary = true;
    second.set(secondSettings);
    check(r, view.updateFromScene(scene) == Status::MultiplePrimary,
          "GC5 multiple primary cameras are rejected");

    second.remove<CS::TagActive>();
    check(r, view.updateFromScene(scene) == Status::Ready,
          "GC6 inactive cameras do not compete");

    settings.front = glm::vec3(0.0f);
    first.set(settings);
    check(r, view.updateFromScene(scene) == Status::Invalid &&
                 view.getFront() == glm::vec3(0.0f, 0.0f, -1.0f) &&
                 view.getFov() == 65.0f,
          "GC7 zero direction is rejected without changing the view");

    settings.front = glm::vec3(0.0f, 1.0f, 0.0f);
    first.set(settings);
    const bool verticalReady = view.updateFromScene(scene) == Status::Ready;
    const auto matrix = view.getViewMatrix();
    bool finiteMatrix = true;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            finiteMatrix = finiteMatrix && std::isfinite(matrix[column][row]);

    check(r, verticalReady && finiteMatrix,
          "GC8 vertical camera produces a finite view matrix");

    settings.fov = 180.0f;
    first.set(settings);
    check(r, view.updateFromScene(scene) == Status::Invalid,
          "GC9 invalid FOV is rejected");

    settings.fov = 65.0f;
    settings.front.x = std::numeric_limits<float>::quiet_NaN();
    first.set(settings);
    check(r, view.updateFromScene(scene) == Status::Invalid,
          "GC10 nonfinite direction is rejected");

    settings.front = glm::vec3(0.0f, 0.0f, -1.0f);
    first.set(settings);
    transform.cachedModelMatrix[3].x =
        std::numeric_limits<float>::infinity();
    first.set(transform);
    check(r, view.updateFromScene(scene) == Status::Invalid,
          "GC11 nonfinite position is rejected");

    scene.destroyEntity(idOf(first));
    check(r, view.updateFromScene(scene) == Status::Missing,
          "GC12 destroyed camera is not retained");

    CS::Scene replacement(engine.getComponentRegistry());
    auto replacementCamera = replacement.createEntity("V_ReplacementCamera");
    replacementCamera.set(CS::CameraComponent{});
    check(r, view.updateFromScene(replacement) == Status::Ready &&
                 view.getFov() == 90.0f,
          "GC13 replacement scene resolves its own camera");
}

inline void validateWorldModes(Engine& engine, Results& r)
{
	check(r, engine.getWorldMode() == WorldMode::Play, "W1 engine starts in Play mode");

	auto* scene = engine.getActiveScene();
	int tagged = 0;
	if (scene)
	{
		scene->getWorld()
			.query_builder()
			.with<CS::GameplaySystem>()
			.with(flecs::Disabled)
			.optional()
			.build()
			.each([&](flecs::entity) { ++tagged; });
	}
	check(r, tagged >= 4, "W0 built-in gameplay systems are tagged");

	engine.setWorldMode(WorldMode::Edit);
	check(r, gameplaySystemsAre(engine, false), "W2 Edit mode disables gameplay systems");
	auto transformSystem = scene ? scene->getWorld().lookup("TransformSystem") : flecs::entity{};
	check(r, transformSystem && !transformSystem.has(flecs::Disabled), "W2 Edit mode keeps editor systems running");

	check(r, engine.loadScene(SAVE_PATH) && gameplaySystemsAre(engine, false),
		  "W3 a scene loaded in Edit mode starts with gameplay off");

	const auto before = engine.getSimulationState();
	engine.stepSimulation();
	check(r, engine.getSimulationState() == before, "W5 stepping is ignored in Edit mode");

	engine.setWorldMode(WorldMode::Play);
      engine.setSimulationState(SimulationState::Running);
      engine.stepSimulation();

      check(r, engine.getSimulationState() == SimulationState::Running,
            "W4 Step is ignored while running");

      engine.setSimulationState(SimulationState::Paused);
	check(r, gameplaySystemsAre(engine, false), "W4 Play + Paused keeps gameplay off");

	auto *stepScene = engine.getActiveScene();
	auto weapon = stepScene->createEntity("V_StepWeapon");
	weapon.set(CS::WeaponComponent{10.0f, 50.0f});
	weapon.set(CS::CameraComponent{});
      auto character = stepScene->createEntity("V_StepCharacter");
      character.set(CS::CharacterControllerComponent{});

	engine.update();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	for (int step = 1; step <= 3; ++step)
      {
      const bool prepared = engine.prepareFrame();
      check(r, prepared, "W6 prepare paused step");
      if (!prepared)
            break;

      engine.stepSimulation();
      engine.stepSimulation(); // A pending request must not queue another tick.
      engine.advanceSimulation(false);

      const float expected =
            static_cast<float>(step) * engine.getPhysicsWorld().fixedDeltaTime();

      const float cooldown =
            weapon.get<CS::WeaponRuntimeComponent>().timeSinceLastShot;

      check(r,
            std::abs(cooldown - expected) < 0.00001f &&
                  engine.getSimulationState() == SimulationState::Paused &&
                  gameplaySystemsAre(engine, false),
            "W6 each Step advances one tick and returns to Paused");

      const auto &characterRuntime =
            character.get<CS::CharacterControllerRuntimeComponent>();
      const auto &characterTransform =
            character.get<CS::TransformComponent>();

      check(r,
            glm::length(
                  characterTransform.position - characterRuntime.currentPos) <
                  0.0001f,
            "W7 stepped character displays its completed physical position");

      engine.update();

      check(r,
            std::abs(
                  weapon.get<CS::WeaponRuntimeComponent>().timeSinceLastShot -
                  expected) < 0.00001f &&
                  engine.getSimulationState() == SimulationState::Paused,
            "W8 the following paused frame adds no extra tick");
      }
      stepScene->destroyEntity(idOf(character));
	stepScene->destroyEntity(idOf(weapon));

	engine.setSimulationState(SimulationState::Running);
	check(r, gameplaySystemsAre(engine, true), "W4 Play + Running turns gameplay on");
}
} // namespace Cthulhu::Validation