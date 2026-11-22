#pragma once

#include "testCommon.hpp"
#include "scene/testsComponents.hpp"
#include "scene/testsDescriptors.hpp"
#include "assets/testsAssets.hpp"
#include "runtime/testsPlayMode.hpp"
#include "runtime/testsRuntimeApi.hpp"
#include "scene/testsSceneFormat.hpp"
#include "scene/testsScene.hpp"
#include "scribe/testsScribe.hpp"
#include "scribe/testsScribeAssets.hpp"
#include "integration/testsEditorDay.hpp"
#include "runtime/testsWorldModes.hpp"
#include "runtime/testsUserSystems.hpp"
#include "regression/testsAuditRegressions.hpp"
#include "scribe/testsEntityClipboard.hpp"

namespace Cthulhu::Validation
{
inline int run(Engine& engine)
{
	Results r;
	check(r, startupRegistrations == 2, "Audit: initialization creates one world before the main scene loads");
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
	validateComponentDescriptors(engine, scene, r);
	validateDescriptorSnapshots(engine, scene, r);
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
    validateScribeDocumentSwitch(engine, r);
	validateScribeEntities(engine, r);
	validateEntityClipboard(engine, r);
	validateScribeTransforms(engine, r);
	validateScribeComponents(engine, r);
	validateScribeModelPlacement(engine, r);
	validateScribeImport(engine, r);
	validateScribeMove(engine, r);
	validateScribeDelete(engine, r);
	validateMissingAssets(engine, r);
	validateReimport(engine, r);
	validateEditorDay(engine, r);
    checkRefInvariant(engine, r, "after scribe entities");

	validateSimStateOnSwitch(engine, r);
	validateGameCamera(engine, r);
	validateWorldModes(engine, r);
	validateInputRouting(engine, r);
	validateSceneSnapshot(engine, r);
	validatePlayStop(engine, r);
	validatePlayCameraRendering(engine, r);
	validateSessionDuringPlay(engine, r);
	validateMainScenePlayTarget(engine, r);
	validateUserSystems(engine, r);
	validateRuntimeApi(engine, r);
	validateSceneFormat(engine, r);
	validateDuplicationGrowth(engine, r);
	validateOrderedDeleteUndo(engine, r);
	validateCommandTransitions(engine, r);
	validateSnapshotTraversal(engine, r);
	validateClampedMerge(engine, r);
	checkRefInvariant(engine, r, "after play/stop");
	validateUnload(engine, r);
	validateShutdown(engine, r);
	Log::Print("==== RESULT: " + std::to_string(r.passed) + " passed, " + std::to_string(r.failed) + " failed ====",
			   "Validation", r.failed ? LogType::LOG_ERROR : LogType::LOG_SUCCESS);
	return r.failed;
}
} // namespace Cthulhu::Validation
