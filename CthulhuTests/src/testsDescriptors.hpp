#pragma once

#include <limits>

#include "componentRegistry.hpp"
#include "testCommon.hpp"

namespace Cthulhu::Validation
{

struct ValidationHealth
{
	float health = 100.0f;
	bool armored = false;
};

inline void validateComponentDescriptors(Engine& engine, CS::Scene& scene, Results& r)
{
	auto& registry = engine.getComponentRegistry();

	check(r, scene.getComponentRegistry() == &registry, "D1 scene uses the engine registry");

	bool allBuiltins = true;
	for (const char* name : {"Transform", "Mesh", "Physics", "Weapon", "AudioSource", "CharacterController", "Camera", "Player"})
	{
		if (!registry.find(name))
		{
			allBuiltins = false;
		}
	}
	check(r, allBuiltins, "D1 every built-in component is registered");

	const auto* transform = registry.find("Transform");
	check(r, transform && transform->core, "D1 Transform is a core component");

	check(r, !registry.registerComponent(CS::makeComponentDescriptor<CS::WeaponComponent>("Weapon")),
		  "D2 duplicate component name rejected");

	CS::ComponentRegistry local;
	auto bad = CS::makeComponentDescriptor<CS::WeaponComponent>("BadEnum");
	bad.fields.push_back(CS::makeField(
		"mode", CS::FieldType::Enum, [](flecs::entity) -> CS::FieldValue { return 0; },
		[](flecs::entity, const CS::FieldValue&) {}));
	check(r, !local.registerComponent(std::move(bad)), "D2 enum field without names rejected");

	flecs::entity w = scene.createEntity("V_Descriptors");
	w.set(CS::WeaponComponent{5.0f, 50.0f});

	auto rate = registry.getField(w, "Weapon", "fireRate");
	check(r, rate && std::holds_alternative<float>(*rate) && std::get<float>(*rate) == 5.0f, "D3 getField reads a value");

	check(r, registry.setField(w, "Weapon", "fireRate", 8.0f) && w.get<CS::WeaponComponent>().fireRate == 8.0f,
		  "D4 setField writes a value");

	check(r, !registry.setField(w, "Weapon", "fireRate", 3), "D5 wrong value type rejected");
	check(r, !registry.setField(w, "Weapon", "fireRate", std::numeric_limits<float>::quiet_NaN()), "D5 NaN rejected");
	check(r, w.get<CS::WeaponComponent>().fireRate == 8.0f, "D5 rejected writes change nothing");

	check(r, registry.setField(w, "Weapon", "fireRate", -4.0f) && w.get<CS::WeaponComponent>().fireRate == 0.0f,
		  "D6 value clamped to field limits");

	check(r, !registry.getField(w, "Physics", "mass"), "D7 getField on a missing component returns nothing");
	check(r, !registry.setField(w, "Weapon", "noSuchField", 1.0f), "D7 unknown field rejected");

	const auto* physics = registry.find("Physics");
	const auto* player = registry.find("Player");
	if (!physics || !player)
	{
		check(r, false, "D8 Physics and Player descriptors exist");
		scene.destroyEntity(idOf(w));
		return;
	}

	const uint32_t base = bodies(engine);
	physics->add(w);
	const auto* rt = w.try_get<CS::PhysicsRuntimeComponent>();
	check(r, rt && rt->bodyId != 0 && bodies(engine) == base + 1, "D8 descriptor add builds runtime");

	const uint32_t oldBody = rt ? rt->bodyId : 0;
	registry.setField(w, "Physics", "type", static_cast<int>(CS::PhysicsBodyType::Dynamic));
	rt = w.try_get<CS::PhysicsRuntimeComponent>();
	check(r, rt && rt->bodyId != oldBody && bodies(engine) == base + 1, "D8 setField fires OnSet (body rebuilt)");
	check(r, !registry.setField(w, "Physics", "type", 7), "D8 enum index out of range rejected");

	physics->remove(w);
	check(r, !w.has<CS::PhysicsRuntimeComponent>() && bodies(engine) == base, "D8 descriptor remove destroys runtime");

	player->add(w);
	const bool tagged = player->has(w);
	player->remove(w);
	check(r, tagged && !player->has(w), "D9 tag component add and remove");

	w.get_mut<CS::TransformComponent>().matrixDirty = false;
	registry.setField(w, "Transform", "position", glm::vec3(1.0f, 2.0f, 3.0f));
	const auto& moved = w.get<CS::TransformComponent>();
	check(r, moved.position == glm::vec3(1.0f, 2.0f, 3.0f) && moved.matrixDirty,
		  "D10 transform setField marks the matrix dirty");

	scene.destroyEntity(idOf(w));
}

inline void validateDescriptorSnapshots(Engine& engine, CS::Scene& scene, Results& r)
{
	auto& registry = engine.getComponentRegistry();

	auto health = CS::makeComponentDescriptor<ValidationHealth>("V_Health");
	health.fields.push_back(CS::makeField(
		"health", CS::FieldType::Float, [](flecs::entity e) -> CS::FieldValue { return e.get<ValidationHealth>().health; },
		[](flecs::entity e, const CS::FieldValue& v) {
			auto c = e.get<ValidationHealth>();
			c.health = std::get<float>(v);
			e.set(c);
		}));
	health.fields.push_back(CS::makeField(
		"armored", CS::FieldType::Bool, [](flecs::entity e) -> CS::FieldValue { return e.get<ValidationHealth>().armored; },
		[](flecs::entity e, const CS::FieldValue& v) {
			auto c = e.get<ValidationHealth>();
			c.armored = std::get<bool>(v);
			e.set(c);
		}));
	check(r, registry.registerComponent(std::move(health)), "S1 user component registers");

	flecs::entity e = scene.createEntity("V_Snapshot");
	e.set(ValidationHealth{42.0f, true});
	e.set(CS::WeaponComponent{5.0f, -5.0f}); // maxRange below the field limit on purpose
	registry.setField(e, "Transform", "position", glm::vec3(4.0f, 5.0f, 6.0f));

	auto dupId = scene.duplicateEntity(idOf(e));
	auto dup = dupId ? scene.findEntity(*dupId) : std::nullopt;
	const auto* dupHealth = dup ? dup->try_get<ValidationHealth>() : nullptr;
	const auto* dupWeapon = dup ? dup->try_get<CS::WeaponComponent>() : nullptr;
	const auto* dupTransform = dup ? dup->try_get<CS::TransformComponent>() : nullptr;
	check(r, dupHealth && dupHealth->health == 42.0f && dupHealth->armored, "S2 duplicate copies user component");
	check(r, dupTransform && dupTransform->position == glm::vec3(4.0f, 5.0f, 6.0f), "S2 duplicate copies transform");
	check(r, dupWeapon && dupWeapon->maxRange == -5.0f, "S3 snapshots keep values exactly (no clamping)");
	if (dupId)
	{
		scene.destroyEntity(*dupId);
	}

	const CS::EntityId id = idOf(e);
	auto snapshot = scene.captureSubtree(id);
	scene.destroyEntity(id);
	const bool restored = snapshot && scene.restoreSubtree(*snapshot);
	auto back = scene.findEntity(id);
	const auto* backHealth = back ? back->try_get<ValidationHealth>() : nullptr;
	check(r, restored && backHealth && backHealth->health == 42.0f, "S4 capture/restore round-trips user component");
	if (back)
	{
		scene.destroyEntity(id);
	}

	CS::EntitySnapshot broken;
	broken.id = CS::generateEntityId();
	broken.name = "V_Broken";
	broken.components.push_back({"NoSuchComponent", {}});
	check(r, !scene.restoreSubtree({broken}), "S5 restore with unknown component fails");
	check(r, !scene.isEntityAlive(broken.id), "S5 failed restore leaves nothing behind");
}

} // namespace Cthulhu::Validation