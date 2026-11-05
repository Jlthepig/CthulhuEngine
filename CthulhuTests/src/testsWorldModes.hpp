#pragma once

#include <chrono>
#include <cmath>
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