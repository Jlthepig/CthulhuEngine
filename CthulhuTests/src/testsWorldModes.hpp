#pragma once

#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

#include "systemRegistry.hpp"
#include "testCommon.hpp"

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
	engine.setSimulationState(SimulationState::Paused);
	check(r, gameplaySystemsAre(engine, false), "W4 Play + Paused keeps gameplay off");

	auto *stepScene = engine.getActiveScene();
	auto weapon = stepScene->createEntity("V_StepWeapon");
	weapon.set(CS::WeaponComponent{10.0f, 50.0f});
	weapon.set(CS::CameraComponent{});

	engine.update();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	engine.stepSimulation();
	engine.update();

	const auto &runtime = weapon.get<CS::WeaponRuntimeComponent>();
	check(r,
		  std::abs(runtime.timeSinceLastShot - engine.getPhysicsWorld().fixedDeltaTime()) < 0.00001f &&
			  engine.getSimulationState() == SimulationState::Paused,
		  "W6 paused step advances weapon cooldown by one simulation tick");

	stepScene->destroyEntity(idOf(weapon));

	engine.setSimulationState(SimulationState::Running);
	check(r, gameplaySystemsAre(engine, true), "W4 Play + Running turns gameplay on");
}
} // namespace Cthulhu::Validation