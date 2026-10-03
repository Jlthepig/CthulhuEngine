#pragma once

#include "testCommon.hpp"
#include "testsComponents.hpp"
#include "testsAssets.hpp"
#include "testsScene.hpp"
#include "testsScribe.hpp"

namespace Cthulhu::Validation
{
inline int run(Engine& engine)
{
	Results r;
	Log::Print("==== STEP 5.5 COMPONENT VALIDATION ====", "Validation", LogType::LOG_INFO);
	if (!engine.hasActiveScene())
	{
		Log::Print("NO ACTIVE SCENE", "Validation", LogType::LOG_ERROR);
		return 1;
	}
	validateAssetManager(engine, r);
	validateAssetTypes(engine, r);
	validateAssetRegistry(engine, r);
	validateModelBounds(engine, r);
	auto& scene = *engine.getActiveScene();
	validatePhysics(engine, scene, r);
	validateCharacter(scene, r);
	validateWeapon(scene, r);
	validateAudio(engine, scene, r);
	validateAudioClips(engine, scene, r);
	validateMeshAndDuplication(engine, scene, r);
	validateMeshLifecycle(engine, scene, r);
	validateRefCounting(engine, scene, r);
	checkRefInvariant(engine, r, "after entity tests");
	validateSave(engine, r);
	validateLoadAndSwitch(engine, r);
	checkRefInvariant(engine, r, "after load/switch");
	validateUnusedCollection(engine, r);
	checkRefInvariant(engine, r, "after unused collection");
	validateFailedLoad(engine, r);
	checkRefInvariant(engine, r, "after failed load");

	validateBadModelInScene(engine, r);
	checkRefInvariant(engine, r, "after bad-model scene");

	validateAssetReferences(engine, r);
	checkRefInvariant(engine, r, "after asset references");

	validateImportSettings(engine, r);
    checkRefInvariant(engine, r, "after import settings");

	validateScribeCore(engine, r);
	validateScribeEntities(engine, r);
    checkRefInvariant(engine, r, "after scribe entities");

	validateSimStateOnSwitch(engine, r);
	validateUnload(engine, r);
	validateShutdown(engine, r);
	Log::Print("==== RESULT: " + std::to_string(r.passed) + " passed, " + std::to_string(r.failed) + " failed ====",
			   "Validation", r.failed ? LogType::LOG_ERROR : LogType::LOG_SUCCESS);
	return r.failed;
}
} // namespace Cthulhu::Validation
