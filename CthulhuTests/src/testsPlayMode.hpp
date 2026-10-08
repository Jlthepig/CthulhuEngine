#pragma once

#include "testCommon.hpp"
#include "testsDescriptors.hpp"
#include "testsEditorDay.hpp"

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
} // namespace Cthulhu::Validation