#pragma once

#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline void validatePhysics(Engine& engine, CS::Scene& scene, Results& r)
{
	const uint32_t base = bodies(engine);
	flecs::entity e = scene.createEntity("V_Physics");
	e.set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.5f), 1.0f});
	const auto* rt = e.try_get<CS::PhysicsRuntimeComponent>();
	check(r, rt && rt->bodyId != 0, "01 add PhysicsComponent -> runtime body exists");
	check(r, bodies(engine) == base + 1, "01 add PhysicsComponent -> exactly one Jolt body");
	const uint32_t oldBody = rt ? rt->bodyId : 0;
	e.set(CS::PhysicsComponent{CS::PhysicsBodyType::Static, glm::vec3(1.0f), 1.0f});
	rt = e.try_get<CS::PhysicsRuntimeComponent>();
	check(r, rt && rt->bodyId != 0 && rt->bodyId != oldBody, "02 modify PhysicsComponent -> body rebuilt");
	check(r, bodies(engine) == base + 1, "02 modify PhysicsComponent -> old body destroyed");
	e.remove<CS::PhysicsComponent>();
	check(r, !e.has<CS::PhysicsRuntimeComponent>(), "03 remove PhysicsComponent -> runtime removed");
	check(r, bodies(engine) == base, "03 remove PhysicsComponent -> Jolt body destroyed");
	flecs::entity parent = scene.createEntity("V_PhysicsParent");
	flecs::entity child = scene.createEntity("V_PhysicsChild");
	parent.set(CS::PhysicsComponent{});
	child.set(CS::PhysicsComponent{});
	scene.setParent(idOf(child), idOf(parent));
	scene.destroyEntity(idOf(parent));
	check(r, bodies(engine) == base, "04 delete physics subtree -> no leaked bodies");
	scene.destroyEntity(idOf(e));
}

inline void validateCharacter(CS::Scene& scene, Results& r)
{
	const int base = characters();
	flecs::entity c = scene.createEntity("V_Character");
	c.set(CS::CharacterControllerComponent{});
	const auto* rt = c.try_get<CS::CharacterControllerRuntimeComponent>();
	check(r, rt && rt->character, "05 add CharacterController -> CharacterVirtual created");
	check(r, characters() == base + 1, "05 add CharacterController -> exactly one CharacterVirtual");
	CS::CharacterControllerComponent config{};
	config.capsuleRadius = 0.5f;
	config.capsuleHeight = 1.5f;
	c.set(config);
	rt = c.try_get<CS::CharacterControllerRuntimeComponent>();
	check(r, rt && rt->character, "06 modify controller -> runtime rebuilt");
	check(r, characters() == base + 1, "06 modify controller -> old CharacterVirtual destroyed once");
	c.remove<CS::CharacterControllerComponent>();
	check(r, !c.has<CS::CharacterControllerRuntimeComponent>(), "05 remove controller -> runtime removed");
	check(r, characters() == base, "05 remove controller -> CharacterVirtual destroyed");
	flecs::entity c2 = scene.createEntity("V_Character2");
	c2.set(CS::CharacterControllerComponent{});
	scene.destroyEntity(idOf(c2));
	check(r, characters() == base, "07 delete character entity -> CharacterVirtual destroyed");
	scene.destroyEntity(idOf(c));
}

inline void validateWeapon(CS::Scene& scene, Results& r)
{
	flecs::entity w = scene.createEntity("V_Weapon");
	w.set(CS::WeaponComponent{5.0f, 50.0f});
	check(r, w.has<CS::WeaponRuntimeComponent>(), "08 add WeaponComponent -> runtime created");
	if (w.has<CS::WeaponRuntimeComponent>())
	{
		auto& rt = w.get_mut<CS::WeaponRuntimeComponent>();
		rt.timeSinceLastShot = 42.0f;
		rt.wantsToFire = true;
	}
	auto dupId = scene.duplicateEntity(idOf(w));
	check(r, dupId.has_value(), "08 duplicate weapon entity");
	if (dupId)
	{
		auto dup = scene.findEntity(*dupId);
		const auto* cfg = dup ? dup->try_get<CS::WeaponComponent>() : nullptr;
		const auto* rt = dup ? dup->try_get<CS::WeaponRuntimeComponent>() : nullptr;
		check(r, cfg && cfg->fireRate == 5.0f && cfg->maxRange == 50.0f, "08 duplicate copies weapon authoring data");
		check(r, rt && rt->timeSinceLastShot == 0.0f && !rt->wantsToFire, "08 duplicate has fresh weapon runtime");
		scene.destroyEntity(*dupId);
	}
	scene.destroyEntity(idOf(w));
}

inline void validateAudio(Engine& engine, CS::Scene& scene, Results& r)
{
	const auto* project = engine.getProject();
	std::optional<std::filesystem::path> resolved;
	if (project)
	{
		resolved = project->resolveResourcePath(TEST_AUDIO);
	}
	if (!resolved || !std::filesystem::exists(*resolved))
	{
		Log::Print("SKIP  09 audio checks (set TEST_AUDIO to a real file)", "Validation", LogType::LOG_WARNING);
		return;
	}
	const size_t base = sounds();
	auto play = [&](flecs::entity e)
	{
		e.get_mut<CS::AudioSourceRuntimeComponent>().playRequested = true;
		scene.getWorld().progress(0.0f);
	};
	flecs::entity a = scene.createEntity("V_Audio");
	a.set(CS::AudioSourceComponent{std::string(TEST_AUDIO), 0.0f, true});
	play(a);
	const auto* rt = a.try_get<CS::AudioSourceRuntimeComponent>();
	check(r, rt && rt->isPlaying && rt->soundInstanceId != 0, "09 play request -> sound playing");
	check(r, sounds() == base + 1, "09 play request -> one active sound");
	a.remove<CS::AudioSourceComponent>();
	check(r, !a.has<CS::AudioSourceRuntimeComponent>(), "09 remove AudioSourceComponent -> runtime removed");
	check(r, sounds() == base, "09 remove AudioSourceComponent -> sound stopped");
	flecs::entity a2 = scene.createEntity("V_Audio2");
	a2.set(CS::AudioSourceComponent{std::string(TEST_AUDIO), 0.0f, true});
	play(a2);
	scene.destroyEntity(idOf(a2));
	check(r, sounds() == base, "09 delete audio entity -> sound stopped");
	scene.destroyEntity(idOf(a));
}

inline void validateMeshAndDuplication(Engine& engine, CS::Scene& scene, Results& r)
{
	std::optional<flecs::entity> meshEntity;
	bool allValid = true;
	int meshCount = 0;
	scene.getWorld().each([&](flecs::entity e, const CS::MeshComponent&)
	{
		++meshCount;
		const auto* rt = e.try_get<CS::MeshRuntimeComponent>();
		const auto* model = rt ? engine.getAssetManager().getModel(rt->model) : nullptr;
		if (!model || model->meshes.empty())
		{
			allValid = false;
		}
		if (!meshEntity)
		{
			meshEntity = e;
		}
	});
	check(r, meshCount > 0, "10 scene contains mesh entities");
	check(r, allValid, "10 every MeshComponent has a valid runtime model");
	if (meshEntity)
	{
		const CS::EntityId srcId = idOf(*meshEntity);
		auto dupId = scene.duplicateEntity(srcId);
		check(r, dupId && *dupId != srcId, "11 duplicate mesh entity -> new UUID");
		if (dupId)
		{
			auto dup = scene.findEntity(*dupId);
			const auto* srcMesh = meshEntity->try_get<CS::MeshComponent>();
			const auto* dupMesh = dup ? dup->try_get<CS::MeshComponent>() : nullptr;
			const auto* dupRt = dup ? dup->try_get<CS::MeshRuntimeComponent>() : nullptr;
			check(r, srcMesh && dupMesh && srcMesh->modelPath == dupMesh->modelPath, "11 duplicate copies mesh authoring data");
			check(r, dupRt && engine.getAssetManager().getModel(dupRt->model), "11 duplicate has a runtime model");
			check(r, dup && dup->has<CS::TagActive>(), "11 duplicate is active/renderable");
			scene.destroyEntity(*dupId);
		}
	}
	const uint32_t base = bodies(engine);
	flecs::entity p = scene.createEntity("V_DupParent");
	flecs::entity c = scene.createEntity("V_DupChild");
	p.set(CS::PhysicsComponent{});
	c.set(CS::PhysicsComponent{});
	scene.setParent(idOf(c), idOf(p));
	auto dupRoot = scene.duplicateEntity(idOf(p));
	check(r, dupRoot && *dupRoot != idOf(p), "11 duplicate subtree -> new root UUID");
	if (dupRoot)
	{
		auto kids = scene.getChildren(*dupRoot);
		check(r, kids.size() == 1 && kids[0] != idOf(c), "11 duplicate subtree -> child copied with new UUID");
		check(r, bodies(engine) == base + 4, "11 duplicate subtree -> fresh body for every copy");
		auto dupParent = scene.findEntity(*dupRoot);
		const auto* srcRt = p.try_get<CS::PhysicsRuntimeComponent>();
		const auto* dupRt = dupParent ? dupParent->try_get<CS::PhysicsRuntimeComponent>() : nullptr;
		check(r, srcRt && dupRt && srcRt->bodyId != dupRt->bodyId, "11 duplicate does not share runtime body");
		scene.destroyEntity(*dupRoot);
	}
	scene.destroyEntity(idOf(p));
	check(r, bodies(engine) == base, "11 cleanup -> no leaked bodies");
}
} // namespace Cthulhu::Validation
