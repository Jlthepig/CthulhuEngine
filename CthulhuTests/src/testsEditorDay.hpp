#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "componentRegistry.hpp"
#include "session.hpp"
#include "testCommon.hpp"
#include "testsScribeAssets.hpp"

namespace Cthulhu::Validation
{
struct EntityPrint
{
	CS::EntityId id;
	std::string name;
	std::optional<CS::EntityId> parent;
	std::vector<CS::ComponentSnapshot> components;
};

struct ScenePrint
{
	std::string name;
	std::vector<EntityPrint> entities;
};

inline ScenePrint fingerprint(CS::Scene& scene)
{
	ScenePrint print{scene.getName(), {}};
	const auto& registry = *scene.getComponentRegistry();

	scene.getWorld().each([&](flecs::entity e, const CS::EntityIdentityComponent& identity) {
		const auto* name = e.try_get<CS::NameComponent>();
		print.entities.push_back(
			{identity.id, name ? name->name : std::string{}, scene.getParent(identity.id), registry.capture(e)});
	});

	std::sort(print.entities.begin(), print.entities.end(), [](const EntityPrint& a, const EntityPrint& b) {
		return a.id.high != b.id.high ? a.id.high < b.id.high : a.id.low < b.id.low;
	});

	return print;
}

inline std::string firstDifference(const ScenePrint& a, const ScenePrint& b)
{
	if (a.name != b.name)
	{
		return "scene name: " + a.name + " vs " + b.name;
	}

	if (a.entities.size() != b.entities.size())
	{
		return "entity count: " + std::to_string(a.entities.size()) + " vs " + std::to_string(b.entities.size());
	}

	for (size_t i = 0; i < a.entities.size(); ++i)
	{
		const auto& x = a.entities[i];
		const auto& y = b.entities[i];

		if (x.id != y.id)
		{
			return "entity IDs differ near " + x.name;
		}
		if (x.name != y.name)
		{
			return "entity name: " + x.name + " vs " + y.name;
		}
		if (x.parent != y.parent)
		{
			return "parent of " + x.name;
		}
		if (x.components.size() != y.components.size())
		{
			return "component count on " + x.name;
		}

		for (size_t c = 0; c < x.components.size(); ++c)
		{
			const auto& cx = x.components[c];
			const auto& cy = y.components[c];

			if (cx.component != cy.component || cx.fields.size() != cy.fields.size())
			{
				return "components on " + x.name + ": " + cx.component + " vs " + cy.component;
			}

			for (size_t f = 0; f < cx.fields.size(); ++f)
			{
				if (cx.fields[f].field != cy.fields[f].field || !(cx.fields[f].value == cy.fields[f].value))
				{
					return x.name + "." + cx.component + "." + cx.fields[f].field;
				}
			}
		}
	}

	return {};
}

inline void expectSame(Results& r, const ScenePrint& actual, const ScenePrint& expected, const std::string& label)
{
	const std::string difference = firstDifference(actual, expected);
	if (!difference.empty())
	{
		Log::Print("DIFFERENCE  " + difference, "Validation", LogType::LOG_WARNING);
	}
	check(r, difference.empty(), label);
}

inline void validateEditorDay(Engine& engine, Results& r)
{
	constexpr std::string_view floorPath = "res://assets/models/Floor.glb";
	constexpr std::string_view cratePath = "res://validation_import/V_Crate.glb";
	constexpr std::string_view dayPath = "res://.cthulhu/validation_editor_day.scene";

	const auto* project = engine.getProject();
	auto floorFile = project ? project->resolveResourcePath(floorPath) : std::nullopt;
	if (!floorFile || !engine.createEmptyScene("V_EditorDay"))
	{
		check(r, false, "X0 editor day setup");
		return;
	}

	cleanImportFolder(engine);
	Scribe::Session session(engine);
	auto* scene = engine.getActiveScene();
	const ScenePrint empty = fingerprint(*scene);
	const uint32_t baseBodies = bodies(engine);
	const int baseCharacters = characters();

	bool allOk = true;
	auto ok = [&](const Scribe::Result& result) { allOk = allOk && result.ok(); };
	auto entity = [&](std::string_view name, std::optional<CS::EntityId> parent = std::nullopt) {
		const auto created = session.createEntity(name, parent);
		allOk = allOk && created.ok();
		return created.id;
	};

	ok(session.renameScene("V_EditorDay_Renamed"));

	const auto root = entity("Root");
	ok(session.setField(root, "Transform", "rotation", glm::vec3(0.0f, 0.7853982f, 0.0f)));

	const auto player = entity("Player", root);
	ok(session.addComponent(player, "Player"));
	ok(session.addComponent(player, "CharacterController"));
	ok(session.setField(player, "CharacterController", "capsuleHeight", 1.8f));
	ok(session.setField(player, "Transform", "position", glm::vec3(1.2345678f, 2.0f, -3.5f))); // needs 9 digits

	const auto gun = entity("Gun", player);
	ok(session.addComponent(gun, "Weapon"));
	session.endMerge();
	ok(session.setField(gun, "Weapon", "fireRate", 11.0f)); // a slider drag
	ok(session.setField(gun, "Weapon", "fireRate", 12.0f));
	ok(session.setField(gun, "Weapon", "fireRate", 13.25f));
	session.endMerge();

	allOk = allOk && session.importAsset(*floorFile, cratePath).ok();
	const auto crate = entity("Crate", root);
	ok(session.addComponent(crate, "Mesh"));
	ok(session.setField(crate, "Mesh", "modelPath", std::string(cratePath)));
	ok(session.addComponent(crate, "Physics"));
	ok(session.setField(crate, "Physics", "type", static_cast<int>(CS::PhysicsBodyType::Dynamic)));
	ok(session.setField(crate, "Physics", "mass", 2.5f));

	const auto copy = session.duplicateEntity(crate);
	allOk = allOk && copy.ok();
	ok(session.setField(copy.id, "Transform", "position", glm::vec3(3.0f, 0.0f, 0.0f)));
	ok(session.reparentEntity(copy.id, std::nullopt));
	ok(session.renameEntity(copy.id, "Crate Copy"));

	const auto temp = entity("Temp");
	ok(session.deleteEntity(temp));

	auto audioFile = project->resolveResourcePath(TEST_AUDIO);
	if (audioFile && std::filesystem::exists(*audioFile))
	{
		const auto speaker = entity("Speaker", root);
		ok(session.addComponent(speaker, "AudioSource"));
		ok(session.setField(speaker, "AudioSource", "filePath", std::string(TEST_AUDIO)));
		ok(session.setField(speaker, "AudioSource", "volume", 0.0f));
		ok(session.setField(speaker, "AudioSource", "loop", true));
	}

	check(r, allOk, "X1 every editor action succeeds through Session");
	check(r, scene->isDirty(), "X1 editing dirties the scene");

	check(r, session.saveAs(dayPath).ok() && !scene->isDirty(), "X2 save marks the scene clean");
	const ScenePrint saved = fingerprint(*scene);

	session.undo();
	const bool dirtyAfterUndo = scene->isDirty();
	session.redo();
	check(r, dirtyAfterUndo && !scene->isDirty(), "X3 one undo dirties, redo returns to clean");
	expectSame(r, fingerprint(*scene), saved, "X3 undo + redo changes nothing");

	while (session.undo())
	{
	}
	expectSame(r, fingerprint(*scene), empty, "X4 undoing everything returns to the empty scene");
	check(r, bodies(engine) == baseBodies && characters() == baseCharacters,
		  "X4 undoing everything leaves no runtime behind");

	while (session.redo())
	{
	}
	expectSame(r, fingerprint(*scene), saved, "X5 redoing everything rebuilds the saved scene");
	check(r, !scene->isDirty(), "X5 redo back to the save point is clean");

	const bool loaded = engine.loadScene(dayPath);
	check(r, loaded, "X6 saved scene loads");
	if (loaded)
	{
		scene = engine.getActiveScene();
		expectSame(r, fingerprint(*scene), saved, "X6 save + load round-trips every field");
		check(r, !scene->isDirty(), "X6 loaded scene is clean");
	}

	engine.loadScene(SAVE_PATH);
	engine.getAssetManager().collectUnusedModels();
	cleanImportFolder(engine);
}
} // namespace Cthulhu::Validation