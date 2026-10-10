#pragma once

#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline void validateSave(Engine& engine, Results& r)
{
	auto& scene = *engine.getActiveScene();
	flecs::entity w = scene.createEntity("V_SavedWeapon");
	w.set(CS::WeaponComponent{7.0f, 70.0f});
	w.get_mut<CS::WeaponRuntimeComponent>().timeSinceLastShot = 99.0f;
	w.get_mut<CS::WeaponRuntimeComponent>().wantsToFire = true;
	flecs::entity ch = scene.createEntity("V_SavedCharacter");
	ch.set(CS::CharacterControllerComponent{});
	ch.get_mut<CS::CharacterControllerRuntimeComponent>().verticalVelocity = 12.0f;
	flecs::entity au = scene.createEntity("V_SavedAudio");
	au.set(CS::AudioSourceComponent{std::string(TEST_AUDIO), 0.25f, false});
	const bool saved = engine.saveActiveSceneAs(SAVE_PATH);
	check(r, saved, "12 save scene");
	check(r, !scene.isDirty(), "12 successful save marks scene clean");
	auto resolved = engine.getProject()->resolveResourcePath(SAVE_PATH);
	const std::string text = resolved ? Utils::FileReader::readFile(resolved->string()) : "";
	bool runtimeLeaked = false;
	for (std::string_view key : {"bodyId", "soundInstanceId", "timeSinceLastShot", "wantsToFire",
								 "verticalVelocity", "pendingMove", "pendingJump", "isPlaying", "playRequested"})
	{
		if (text.find(key) != std::string::npos)
		{
			runtimeLeaked = true;
		}
	}
	check(r, !text.empty(), "12 saved file readable");
	check(r, !runtimeLeaked, "12 saved file contains no runtime state");
	check(r, text.find("\"CharacterController\"") != std::string::npos &&
			 text.find("\"Weapon\"") != std::string::npos, "12 saved file contains authoring components");
}

inline void validateLoadAndSwitch(Engine& engine, Results& r)
{
	const size_t modelsBefore = engine.getAssetManager().getLoadedModelCount();
	const bool loaded = engine.loadScene(SAVE_PATH);
	check(r, loaded, "13 load saved scene");
	check(r, engine.getAssetManager().getLoadedModelCount() == modelsBefore, "C1 scene switch reuses loaded models");
	if (!loaded)
	{
		return;
	}
	auto& scene = *engine.getActiveScene();
	auto& world = scene.getWorld();
	const int physicsEntities = world.count<CS::PhysicsComponent>();
	check(r, physicsEntities == world.count<CS::PhysicsRuntimeComponent>(), "13 every PhysicsComponent has runtime after load");
	check(r, bodies(engine) == static_cast<uint32_t>(physicsEntities), "15 old scene's bodies destroyed on switch");
	const int controllers = world.count<CS::CharacterControllerComponent>();
	check(r, characters() == controllers, "15 old scene's CharacterVirtuals destroyed on switch");
	bool weaponOk = false;
	bool characterOk = false;
	bool audioOk = false;
	world.each([&](flecs::entity e, const CS::NameComponent& name)
	{
		if (name.name == "V_SavedWeapon")
		{
			const auto* cfg = e.try_get<CS::WeaponComponent>();
			const auto* rt = e.try_get<CS::WeaponRuntimeComponent>();
			weaponOk = cfg && cfg->fireRate == 7.0f && cfg->maxRange == 70.0f &&
					   rt && rt->timeSinceLastShot == 0.0f && !rt->wantsToFire;
		}
		else if (name.name == "V_SavedAudio")
		{
			const auto* cfg = e.try_get<CS::AudioSourceComponent>();
			const auto* rt = e.try_get<CS::AudioSourceRuntimeComponent>();
			audioOk = cfg && cfg->volume == 0.25f && !cfg->loop &&
					  rt && engine.getAssetManager().getAudioClip(rt->clip) && !rt->isPlaying;
		}
		else if (name.name == "V_SavedCharacter")
		{
			const auto* rt = e.try_get<CS::CharacterControllerRuntimeComponent>();
			characterOk = rt && rt->character && rt->verticalVelocity == 0.0f;
		}
	});
	check(r, weaponOk, "13 weapon authoring restored with fresh runtime");
	check(r, characterOk, "13 character controller restored with fresh runtime");
	check(r, audioOk, "I1 audio source restored with a live clip and fresh runtime");
	check(r, !scene.isDirty(), "13 loaded scene is clean");
}

inline void validateFailedLoad(Engine& engine, Results& r)
{
	auto resolved = engine.getProject()->resolveResourcePath(BROKEN_PATH);
	if (!resolved)
	{
		check(r, false, "13 resolve broken scene path");
		return;
	}
	{
		std::ofstream out(*resolved, std::ios::trunc);
		out << R"({
	"format_version": 2,
	"name": "validation_broken",
	"entities": [
		{ "id": "0123456789abcdef0123456789abcdef", "name": "Good",
					"model": "res://assets/models/Floor.glb",
		  "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1],
		  "physics": { "type": "static", "half_extent": [1,1,1] },
					  "audio": { "file": "res://assets/audio/gunshot.wav", "volume": 0.0, "loop": false },
		  "character_controller": {} },
		{ "id": "fedcba9876543210fedcba9876543210", "name": "Bad",
		  "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1],
		  "physics": { "type": "wobbly", "half_extent": [1,1,1] } }
	],
	"directional_light": { "direction": [0,-1,0], "color": [1,1,1], "intensity": 1 },
	"point_lights": []
})";
	}
	const CS::Scene* before = engine.getActiveScene();
	const uint32_t b = bodies(engine);
	const int c = characters();
	auto& assets = engine.getAssetManager();
	const Assets::ModelHandle floor = assets.loadModel("res://assets/models/Floor.glb");
	const uint32_t floorRefs = assets.getModelRefCount(floor);
	const Assets::AudioClipHandle clip = assets.loadAudioClip(TEST_AUDIO);
	const uint32_t clipRefs = assets.getAudioClipRefCount(clip);
	const bool loaded = engine.loadScene(BROKEN_PATH);
	check(r, !loaded, "13 broken scene is rejected");
	check(r, engine.getActiveScene() == before, "13 failed load keeps current scene active");
	check(r, bodies(engine) == b, "13 failed load leaks no Jolt bodies");
	check(r, characters() == c, "13 failed load leaks no CharacterVirtuals");
	check(r, assets.getModelRefCount(floor) == floorRefs, "F2 failed load releases its model references");
	check(r, assets.getAudioClipRefCount(clip) == clipRefs, "F2 failed load releases its audio clip references");
}

inline void validateSimStateOnSwitch(Engine& engine, Results& r)
{
	engine.setSimulationState(SimulationState::Paused);
	const bool loaded = engine.loadScene(SAVE_PATH);
	bool disabled = false;
	if (loaded)
	{
		auto system = engine.getActiveScene()->getWorld().lookup("PhysicsSyncSystem");
		disabled = system && system.has(flecs::Disabled);
	}
	check(r, loaded && disabled, "15 new scene respects paused simulation state");
	engine.setSimulationState(SimulationState::Running);
}

inline void validateUnload(Engine& engine, Results& r)
{
	engine.unloadScene();
	check(r, !engine.hasActiveScene(), "14 unload -> no active scene");
	check(r, bodies(engine) == 0, "14 unload -> all Jolt bodies destroyed");
	check(r, characters() == 0, "14 unload -> all CharacterVirtuals destroyed");
	check(r, sounds() == 0, "14 unload -> all sounds stopped");
	check(r, engine.getAssetManager().getTotalModelRefCount() == 0, "14 unload -> all model references released");
	check(r, engine.getAssetManager().getLoadedModelCount() == 0, "14 unload -> all unused models freed");
	check(r, engine.getAssetManager().getTotalAudioClipRefCount() == 0 &&
			 engine.getAssetManager().getLoadedAudioClipCount() == 0, "14 unload -> audio clips released and freed");
	engine.createEmptyScene("Validation");
}

inline void validateShutdown(Engine& engine, Results& r)
{
	auto& scene = *engine.getActiveScene();
	flecs::entity p = scene.createEntity("V_ShutdownPhysics");
	p.set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.5f), 1.0f});
	flecs::entity c = scene.createEntity("V_ShutdownCharacter");
	c.set(CS::CharacterControllerComponent{});
	engine.shutdown();
	check(r, engine.getState() == EngineState::Uninitialized, "16 shutdown completes");
	check(r, characters() == 0, "16 shutdown destroys all CharacterVirtuals");
	check(r, sounds() == 0, "16 shutdown leaves no active sounds");
	check(r, engine.getAssetManager().getLoadedModelCount() == 0, "16 shutdown frees all model assets");
	check(r, engine.getAssetManager().getLoadedAudioClipCount() == 0, "16 shutdown frees all audio clips");
}
} // namespace Cthulhu::Validation
