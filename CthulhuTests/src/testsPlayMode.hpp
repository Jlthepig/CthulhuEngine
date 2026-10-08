#pragma once

#include "testCommon.hpp"
#include "testsDescriptors.hpp"
#include "testsEditorDay.hpp"
#include "testsWorldModes.hpp"

namespace Cthulhu::Validation
{

inline void validateSceneSnapshot(Engine& engine, Results& r)
{
	if (!engine.loadScene(SAVE_PATH))
	{
		check(r, false, "P0 snapshot setup");
		return;
	}

	auto* scene = engine.getActiveScene();

	flecs::entity health = scene->createEntity("V_SnapshotHealth");
	health.set(ValidationHealth{33.0f, true});
	scene->addPointLight(Rendering::PointLight{});

	const ScenePrint before = fingerprint(*scene);
	const auto path = scene->getResourcePath();
	const size_t lights = scene->getPointLights().size();

	const CS::SceneSnapshot snapshot = scene->captureScene();
	check(r, snapshot.entities.size() == before.entities.size(), "P1 snapshot captures every entity");

	engine.createEmptyScene("V_SnapshotTarget");
	scene = engine.getActiveScene();

	check(r, scene->restoreScene(snapshot), "P2 snapshot restores into an empty scene");
	expectSame(r, fingerprint(*scene), before, "P2 restored scene matches field for field");
	check(r, scene->getResourcePath() == path && scene->getPointLights().size() == lights && scene->isDirty(),
		  "P3 path, lights and dirty flag restored");

	auto& world = scene->getWorld();
	const int physicsEntities = world.count<CS::PhysicsComponent>();
	check(r, physicsEntities == world.count<CS::PhysicsRuntimeComponent>() &&
				 bodies(engine) == static_cast<uint32_t>(physicsEntities),
		  "P4 restored scene builds fresh runtime");

	check(r, !scene->restoreScene(snapshot), "P5 restoring into a non-empty scene is refused");

	engine.loadScene(SAVE_PATH);
}

inline void validatePlayStop(Engine& engine, Results& r)
{
	if (!engine.loadScene(SAVE_PATH))
	{
		check(r, false, "T0 play test setup");
		return;
	}
	engine.setWorldMode(WorldMode::Edit);

	auto* editScene = engine.getActiveScene();
	editScene->addPointLight(Rendering::PointLight{}); 

	
	Assets::ModelHandle modelBefore{};
	std::optional<CS::EntityId> meshEntity;
	editScene->getWorld().each([&](flecs::entity e, const CS::MeshRuntimeComponent& mesh) {
		if (!meshEntity)
		{
			meshEntity = idOf(e);
			modelBefore = mesh.model;
		}
	});

	const ScenePrint before = fingerprint(*editScene);
	const uint64_t generation = engine.getSceneGeneration();

	check(r, engine.play() && engine.isPlaying() && engine.getWorldMode() == WorldMode::Play &&
				 gameplaySystemsAre(engine, true),
		  "T1 play switches to Play with gameplay on");

	auto* playScene = engine.getActiveScene();
	expectSame(r, fingerprint(*playScene), before, "T2 the play copy matches the edit scene");
	check(r, engine.getSceneGeneration() == generation, "T3 play keeps the same document generation");
	check(r, !engine.play(), "T4 play while playing is refused");
	check(r, !engine.saveActiveScene() && !engine.saveActiveSceneAs("res://.cthulhu/validation_play.scene"),
		  "T5 saving while playing is refused");

	
	if (meshEntity)
	{
		if (auto e = playScene->findEntity(*meshEntity))
		{
			auto t = e->get<CS::TransformComponent>();
			t.position += glm::vec3(5.0f);
			e->set(t);
		}
	}
	flecs::entity spawned = playScene->createEntity("V_Spawned");
	spawned.set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.5f), 1.0f});
	if (!before.entities.empty())
	{
		playScene->destroyEntity(before.entities.back().id);
	}

	check(r, engine.stop() && !engine.isPlaying() && engine.getWorldMode() == WorldMode::Edit &&
				 gameplaySystemsAre(engine, false),
		  "T6 stop returns to Edit with gameplay off");

	auto* restored = engine.getActiveScene();
	expectSame(r, fingerprint(*restored), before, "T7 stop restores the edit scene exactly");

	auto& world = restored->getWorld();
	check(r, bodies(engine) == static_cast<uint32_t>(world.count<CS::PhysicsComponent>()) &&
				 characters() == world.count<CS::CharacterControllerComponent>(),
		  "T8 nothing from play is left in physics");
	check(r, restored->isDirty() && engine.getSceneGeneration() == generation,
		  "T9 dirty flag and document generation survive");
	check(r, !engine.stop(), "T10 stop while editing is refused");

	auto restoredMesh = meshEntity ? restored->findEntity(*meshEntity) : std::nullopt;
	const auto* meshRuntime = restoredMesh ? restoredMesh->try_get<CS::MeshRuntimeComponent>() : nullptr;
	check(r, meshRuntime && meshRuntime->model == modelBefore, "T11 models are not reloaded");

	engine.play();
	engine.createEmptyScene("V_Level2"); 
	engine.stop();
	expectSame(r, fingerprint(*engine.getActiveScene()), before,
			   "T12 stop after a level switch still returns to the edit scene");

	engine.setWorldMode(WorldMode::Play);
	engine.loadScene(SAVE_PATH);
}
} // namespace Cthulhu::Validation