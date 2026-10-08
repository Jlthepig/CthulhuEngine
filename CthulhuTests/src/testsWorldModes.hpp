#pragma once

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
	engine.setSimulationState(SimulationState::Running);
	check(r, gameplaySystemsAre(engine, true), "W4 Play + Running turns gameplay on");
}
} // namespace Cthulhu::Validation