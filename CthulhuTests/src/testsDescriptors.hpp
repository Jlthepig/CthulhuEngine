#pragma once

#include <limits>

#include "componentRegistry.hpp"
#include "session.hpp"
#include "testCommon.hpp"

namespace Cthulhu::Validation
{

struct ValidationHealth
{
	float health = 100.0f;
	bool armored = false;
};

struct ValidationInteger
{
	int value = 0;
};

inline void validateComponentDescriptors(Engine& engine, CS::Scene& scene, Results& r)
{
	auto& registry = engine.getComponentRegistry();

	{
		int writes = 0;
		flecs::world world;
		auto entity = world.entity().set(ValidationHealth{12.5f, true});

		auto observer = world.observer<ValidationHealth>()
			.event(flecs::OnSet)
			.each([&](ValidationHealth &) { ++writes; });

		auto field =
			CS::makeField<ValidationHealth, &ValidationHealth::health>(
				"health", CS::FieldType::Float);

		field.set(entity, CS::FieldValue{25.0f});

		check(r, std::get<float>(field.get(entity)) == 25.0f &&
					 entity.get<ValidationHealth>().armored && writes == 1,
			  "Member field updates one member and emits OnSet");

		observer.destruct();
	}

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
	check(r, rt && rt->bodyId.has_value() && bodies(engine) == base + 1, "D8 descriptor add builds runtime");

	const auto oldBody = rt ? rt->bodyId : std::nullopt;
	registry.setField(w, "Physics", "type", static_cast<int>(CS::PhysicsBodyType::Dynamic));
	rt = w.try_get<CS::PhysicsRuntimeComponent>();
	check(r, rt && rt->bodyId.has_value() && rt->bodyId != oldBody && bodies(engine) == base + 1,
		  "D8 setField fires OnSet (body rebuilt)");
	check(r, !registry.setField(w, "Physics", "type", 7), "D8 enum index out of range rejected");

	const auto bodyBeforeMassEdit = rt ? rt->bodyId : std::nullopt;
	const bool massEdited = registry.setField(w, "Physics", "mass", 4.0f);
	rt = w.try_get<CS::PhysicsRuntimeComponent>();

	check(r, massEdited && w.get<CS::PhysicsComponent>().mass == 4.0f &&
				 rt && rt->bodyId.has_value() &&
				 rt->bodyId != bodyBeforeMassEdit && bodies(engine) == base + 1,
		  "Member field mass edit rebuilds one physics body");

	physics->remove(w);
	check(r, !w.has<CS::PhysicsRuntimeComponent>() && bodies(engine) == base, "D8 descriptor remove destroys runtime");

	player->add(w);
	const bool tagged = player->has(w);
	player->remove(w);
	check(r, tagged && !player->has(w), "D9 tag component add and remove");

	registry.setField(w, "Transform", "position", glm::vec3(1.0f, 2.0f, 3.0f));
	const auto& moved = w.get<CS::TransformComponent>();
	check(r, moved.position == glm::vec3(1.0f, 2.0f, 3.0f),
		  "D10 transform setField writes position");

	auto integer = CS::makeComponentDescriptor<ValidationInteger>("V_IntegerLimits");
	auto limits = CS::makeField(
		"wide",
		CS::FieldType::Int,
		[](flecs::entity e) -> CS::FieldValue {
			return e.get<ValidationInteger>().value;
		},
		[](flecs::entity e, const CS::FieldValue& value) {
			e.set(ValidationInteger{std::get<int>(value)});
		},
		-std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max());

	integer.fields.push_back(limits);
	limits.name = "fractional";
	limits.min = 1.5f;
	limits.max = 5.5f;
	integer.fields.push_back(limits);

	const bool registered = local.registerComponent(std::move(integer));
	w.set(ValidationInteger{});

	check(r, registered && local.setField(w, "V_IntegerLimits", "wide", 42) &&
				 w.get<ValidationInteger>().value == 42,
		  "Integer limits: wide bounds preserve ordinary values");

	check(r, local.setField(w, "V_IntegerLimits", "wide", std::numeric_limits<int>::max()) &&
				 w.get<ValidationInteger>().value == std::numeric_limits<int>::max(),
		  "Integer limits: wide bounds preserve INT_MAX");

	check(r, local.setField(w, "V_IntegerLimits", "wide", std::numeric_limits<int>::lowest()) &&
				 w.get<ValidationInteger>().value == std::numeric_limits<int>::lowest(),
		  "Integer limits: wide bounds preserve INT_MIN");

	check(r, local.setField(w, "V_IntegerLimits", "fractional", -10) &&
				 w.get<ValidationInteger>().value == 1,
		  "Integer limits: fractional minimum keeps truncation behaviour");

	check(r, local.setField(w, "V_IntegerLimits", "fractional", 10) &&
				 w.get<ValidationInteger>().value == 5,
		  "Integer limits: fractional maximum keeps truncation behaviour");

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
	registry.setField(e, "Transform", "shear", glm::vec3(0.25f, -0.5f, 0.75f));

	auto dupId = scene.duplicateEntity(idOf(e));
	auto dup = dupId ? scene.findEntity(*dupId) : std::nullopt;
	const auto* dupHealth = dup ? dup->try_get<ValidationHealth>() : nullptr;
	const auto* dupWeapon = dup ? dup->try_get<CS::WeaponComponent>() : nullptr;
	const auto* dupTransform = dup ? dup->try_get<CS::TransformComponent>() : nullptr;
	check(r, dupHealth && dupHealth->health == 42.0f && dupHealth->armored, "S2 duplicate copies user component");
	check(r, dupTransform && dupTransform->position == glm::vec3(4.0f, 5.0f, 6.0f) &&
				 dupTransform->shear == glm::vec3(0.25f, -0.5f, 0.75f),
		  "S2 duplicate copies transform");
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
	const auto* backTransform = back ? back->try_get<CS::TransformComponent>() : nullptr;
	check(r, restored && backHealth && backHealth->health == 42.0f, "S4 capture/restore round-trips user component");
	check(r, backTransform && backTransform->shear == glm::vec3(0.25f, -0.5f, 0.75f),
		  "S4 capture/restore round-trips shear");
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

inline void validateScribeComponents(Engine& engine, Results& r)
{
	using Scribe::ChangeType;
	using Status = Scribe::Result::Status;

	Scribe::Session session(engine);

	auto hasEvent = [](const std::vector<Scribe::ChangeEvent>& events, ChangeType type, std::string_view component) {
		for (const auto& e : events)
		{
			if (e.type == type && e.component == component)
			{
				return true;
			}
		}
		return false;
	};

	auto* scene = engine.getActiveScene();
	auto created = session.createEntity("V_ScribeComponents");
	auto entity = scene ? scene->findEntity(created.id) : std::nullopt;
	if (!created.ok() || !entity)
	{
		check(r, false, "C0 entity created for component commands");
		return;
	}
	const CS::EntityId id = created.id;
	(void)session.takeEvents();

	auto weapon = [&]() { return entity->try_get<CS::WeaponComponent>(); };

	check(r, session.addComponent(id, "Weapon").status == Status::Applied && weapon() &&
				 entity->has<CS::WeaponRuntimeComponent>(),
		  "C1 addComponent adds component and runtime");
	check(r, hasEvent(session.takeEvents(), ChangeType::ComponentAdded, "Weapon"), "C1 addComponent emits ComponentAdded");
	check(r, session.addComponent(id, "Weapon").status == Status::NoChange, "C1 adding an existing component is NoChange");
	check(r, !session.addComponent(id, "Transform").ok() && !session.addComponent(id, "NoSuchComponent").ok(),
		  "C2 core and unknown components rejected");

	session.endMerge();
	check(r, session.setField(id, "Weapon", "fireRate", 7.0f).status == Status::Applied && weapon() &&
				 weapon()->fireRate == 7.0f,
		  "C3 setField applies");
	check(r, hasEvent(session.takeEvents(), ChangeType::ComponentChanged, "Weapon"), "C3 setField emits ComponentChanged");
	check(r, session.undoName() == "Set Weapon.fireRate", "C3 undo label names the field");
	check(r, session.setField(id, "Weapon", "fireRate", 7.0f).status == Status::NoChange, "C4 same value is NoChange");

	session.endMerge();
	session.setField(id, "Weapon", "fireRate", 8.0f);
	session.setField(id, "Weapon", "fireRate", 9.0f); // like a slider drag
	session.undo();
	const bool mergedBack = weapon() && weapon()->fireRate == 7.0f;
	session.undo();
	check(r, mergedBack && weapon() && weapon()->fireRate == 10.0f, "C5 a drag merges into one undo step");

	session.endMerge();
	session.setField(id, "Weapon", "fireRate", -3.0f);
	const bool clamped = weapon() && weapon()->fireRate == 0.0f;
	session.undo();
	check(r, clamped && weapon() && weapon()->fireRate == 10.0f, "C6 clamped edit undoes to the old value");

	const std::string labelBefore(session.undoName());
	check(r, !session.setField(id, "Weapon", "fireRate", 3).ok() && session.undoName() == labelBefore,
		  "C7 wrong value type fails and records nothing");

	session.endMerge();
	session.setField(id, "Weapon", "maxRange", 77.0f);
	(void)session.takeEvents();
	const bool removed = session.removeComponent(id, "Weapon").status == Status::Applied && !weapon() &&
						 !entity->has<CS::WeaponRuntimeComponent>();
	check(r, removed && hasEvent(session.takeEvents(), ChangeType::ComponentRemoved, "Weapon"),
		  "C8 removeComponent removes and emits");
	session.undo();
	check(r, weapon() && weapon()->maxRange == 77.0f, "C8 undo remove restores field values");
	session.redo();
	check(r, !weapon(), "C9 redo remove removes again");
	check(r, session.removeComponent(id, "Weapon").status == Status::NoChange, "C9 removing a missing component is NoChange");

	const bool userAdded = session.addComponent(id, "V_Health").status == Status::Applied &&
						   session.setField(id, "V_Health", "health", 5.0f).status == Status::Applied;
	const auto* hp = entity->try_get<ValidationHealth>();
	check(r, userAdded && hp && hp->health == 5.0f, "C10 user component works through Scribe");

	session.deleteEntity(id);
}

} // namespace Cthulhu::Validation