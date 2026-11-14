#pragma once

#include <algorithm>
#include <vector>

#include "session.hpp"
#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline constexpr std::string_view IMPORT_DIR = "res://validation_import";
inline constexpr std::string_view IMPORT_PATH =
	"res://validation_import/caf\xC3\xA9/\xE9\x9F\xB3.wav";

// deletes the import test folder and forgets its records so runs have nothing left behind
inline void cleanImportFolder(Engine& engine)
{
	const auto* project = engine.getProject();
	auto folder = project ? project->resolveResourcePath(IMPORT_DIR) : std::nullopt;
	if (!folder)
	{
		return;
	}

	std::error_code error;
	std::filesystem::remove_all(*folder, error);

	auto& assets = engine.getAssetManager();
	assets.refreshRegistry();

	std::vector<Assets::AssetId> stale;
	for (const auto& record : assets.getRegistry().getRecords())
	{
		if (record.missing && record.path.starts_with(IMPORT_DIR))
		{
			stale.push_back(record.id);
		}
	}

	for (const auto id : stale)
	{
		assets.getRegistry().forget(id);
	}
}

inline void validateScribeModelPlacement(Engine &engine, Results &r)
{
	constexpr std::string_view path = "res://assets/models/DamagedHelmet.glb";
	auto *scene = engine.getActiveScene();
	const auto &assets = engine.getAssetManager();
	const auto *record = assets.getRegistry().findByPath(path);
	if (!scene || !record)
	{
		check(r, false, "Placement needs the DamagedHelmet.glb fixture");
		return;
	}

	Scribe::Session session(engine);
	const bool wasDirty = scene->isDirty();
	const auto refs = assets.getTotalModelRefCount();
	const auto entityCount = scene->getWorld().count<CS::EntityIdentityComponent>();
	const glm::vec3 position{1.25f, 2.5f, -3.0f};
	const auto rejected = session.placeModel({}, position);
	check(r, !rejected.ok() && !rejected.id.isValid() && !session.canUndo() &&
			 session.takeEvents().empty() && scene->isDirty() == wasDirty &&
			 assets.getTotalModelRefCount() == refs &&
			 scene->getWorld().count<CS::EntityIdentityComponent>() == entityCount,
		  "Placement rejects an unknown asset without changing the scene");

	const auto placed = session.placeModel(record->id, position);
	const auto matchesPlacement = [&]() {
		const auto entity = scene->findEntity(placed.id);
		if (!entity || scene->getParent(placed.id))
			return false;

		const auto *name = entity->try_get<CS::NameComponent>();
		const auto *transform = entity->try_get<CS::TransformComponent>();
		const auto *mesh = entity->try_get<CS::MeshComponent>();
		const auto *runtime = entity->try_get<CS::MeshRuntimeComponent>();
		return name && name->name == "DamagedHelmet" &&
			   transform && transform->position == position &&
			   mesh && mesh->modelPath == path &&
			   runtime && assets.getModel(runtime->model) &&
			   assets.getTotalModelRefCount() == refs + 1;
	};

	const auto events = session.takeEvents();
	check(r, placed.ok() && matchesPlacement() && scene->isDirty() &&
			 events.size() == 1 && events.front().type == Scribe::ChangeType::EntityCreated &&
			 events.front().entityId == placed.id,
		  "Placement publishes one complete root entity");

	const bool undone = session.undo();
	check(r, undone && !scene->isEntityAlive(placed.id) && !session.canUndo() &&
			 assets.getTotalModelRefCount() == refs,
		  "Placement undoes in one step and releases its model reference");

	const bool redone = session.redo();
	check(r, redone && matchesPlacement(), "Placement redo restores the same entity and position");
	if (redone)
		(void)session.undo();
	if (wasDirty)
		scene->markDirty();
}

inline void validateScribeImport(Engine& engine, Results& r)
{
	const auto* project = engine.getProject();
	auto source = project ? project->resolveResourcePath(TEST_AUDIO) : std::nullopt;
	if (!source || !std::filesystem::exists(*source))
	{
		Log::Print("SKIP  I import checks (set TEST_AUDIO to a real file)", "Validation", LogType::LOG_WARNING);
		return;
	}

	cleanImportFolder(engine);

	Scribe::Session session(engine);
	(void) session.takeEvents();
	auto& assets = engine.getAssetManager();

	const auto staged = project->getRootPath() / "validation_import" /
		std::filesystem::path(u8"source_caf\u00E9/\u97F3.wav");

	std::error_code copyError;
	std::filesystem::create_directories(staged.parent_path(), copyError);
	const bool copied = !copyError && std::filesystem::copy_file(
		*source, staged, std::filesystem::copy_options::none, copyError);

	check(r, copied && !copyError, "I0 Unicode source fixture");
	if (!copied || copyError)
	{
		cleanImportFolder(engine);
		return;
	}
	source = staged;

	auto imported = session.importAsset(*source, IMPORT_PATH);
	const auto* record = imported.ok() ? assets.getRegistry().findById(imported.id) : nullptr;
	auto target = project->resolveResourcePath(IMPORT_PATH);
	check(r, imported.ok() && imported.id.isValid() && target && std::filesystem::exists(*target),
		  "I1 import copies the file into the project");
	check(r, std::filesystem::is_regular_file(*source),
		  "I1 import preserves its source");
	check(r, record && record->path == IMPORT_PATH && record->type == Assets::AssetType::Audio && !record->missing,
		  "I1 import registers the asset");

	bool sawEvent = false;
	for (const auto& e : session.takeEvents())
	{
		if (e.type == Scribe::ChangeType::AssetImported && e.assetId == imported.id)
		{
			sawEvent = true;
		}
	}
	check(r, sawEvent, "I1 import emits AssetImported");

	Assets::AssetRegistry reloaded;
	const auto* onDisk = reloaded.load(project->getRootPath() / ".cthulhu" / "assets.json")
							 ? reloaded.findById(imported.id) : nullptr;
	check(r, onDisk && onDisk->path == IMPORT_PATH, "I2 import is saved to the registry file");

	check(r, assets.refreshRegistry().ok &&
			 assets.getRegistry().findByPath(IMPORT_PATH) &&
			 assets.getRegistry().findByPath(IMPORT_PATH)->id == imported.id,
		  "I2 Unicode scan preserves the asset ID");

	check(r, !Assets::normaliseResourcePath("res://bad_\xC0\xAF.wav") &&
			 !Assets::normaliseResourcePath(
				 std::string_view("res://bad\0.wav", 14)),
		  "I2 malformed UTF-8 and embedded NUL paths are rejected");

	auto clip = assets.acquireAudioClip(IMPORT_PATH);
	check(r, clip.isValid(), "I3 imported clip loads");
	assets.releaseAudioClip(clip);
	assets.collectUnusedAudioClips();

	check(r, !session.importAsset(*source, IMPORT_PATH).ok(), "I4 import never overwrites");
	check(r, !session.importAsset(*source, "res://validation_import/V_Bad.txt").ok(), "I5 unknown type rejected");
	check(r, !session.importAsset(*source, "res://validation_import/V_Bad.glb").ok(), "I5 mismatched type rejected");
	check(r, !session.importAsset(*source, "res://validation_import/.hidden/V_Bad.wav").ok(),
		  "I5 hidden folder rejected");
	check(r, !session.importAsset(*source, "res://../V_Bad.wav").ok(), "I5 path outside the project rejected");
	check(r, !session.importAsset(project->getRootPath() / "no_such_file.wav", "res://validation_import/V_Gone.wav").ok(),
		  "I5 missing source rejected");

	cleanImportFolder(engine);
	check(r, assets.getRegistry().findByPath(IMPORT_PATH) == nullptr, "I6 cleanup leaves the registry clean");
}

inline void validateScribeMove(Engine& engine, Results& r)
{
	using Status = Scribe::Result::Status;
	constexpr std::string_view floorPath = "res://assets/models/Floor.glb";
	constexpr std::string_view startPath =
		"res://validation_import/caf\xC3\xA9/V_Move.glb";
	constexpr std::string_view movedPath =
		"res://validation_import/moved_caf\xC3\xA9/V_Moved.glb";

	const auto* project = engine.getProject();
	auto* scene = engine.getActiveScene();
	auto source = project ? project->resolveResourcePath(floorPath) : std::nullopt;
	if (!scene || !source)
	{
		check(r, false, "M0 move test setup");
		return;
	}

	cleanImportFolder(engine);
	Scribe::Session session(engine);
	auto& assets = engine.getAssetManager();

	auto imported = session.importAsset(*source, startPath);
	auto created = session.createEntity("V_MoveUser");
	session.addComponent(created.id, "Mesh");
	session.setField(created.id, "Mesh", "modelPath", std::string(startPath));
	auto entity = scene->findEntity(created.id);
	if (!imported.ok() || !entity || !entity->has<CS::MeshRuntimeComponent>())
	{
		check(r, false, "M0 move test setup");
		session.deleteEntity(created.id);
		cleanImportFolder(engine);
		return;
	}

	auto meshPath = [&]() { return entity->get<CS::MeshComponent>().modelPath; };

	const size_t loadedBefore = assets.getLoadedModelCount();
	(void) session.takeEvents();

	const auto moved = session.moveAsset(imported.id, movedPath);
	const auto* record = assets.getRegistry().findById(imported.id);
	auto oldFile = project->resolveResourcePath(startPath);
	auto newFile = project->resolveResourcePath(movedPath);
	check(r, moved.status == Status::Applied && oldFile && newFile && !std::filesystem::exists(*oldFile) &&
				 std::filesystem::exists(*newFile),
		  "M1 move moves the file");
	check(r, record && record->path == movedPath, "M1 record keeps its ID and gets the new path");

	Assets::AssetRegistry reloaded;
	const auto* onDisk = reloaded.load(project->getRootPath() / ".cthulhu" / "assets.json")
						 ? reloaded.findById(imported.id) : nullptr;
	check(r, onDisk && onDisk->path == movedPath, "M2 move is saved to the registry file");

	check(r, meshPath() == movedPath, "M3 open scene reference follows the move");
	check(r, entity->has<CS::MeshRuntimeComponent>() && assets.getLoadedModelCount() == loadedBefore,
		  "M3 loaded model is reused, not reloaded");

	bool sawMoved = false;
	bool sawChanged = false;
	for (const auto& e : session.takeEvents())
	{
		sawMoved = sawMoved || (e.type == Scribe::ChangeType::AssetMoved && e.assetId == imported.id);
		sawChanged = sawChanged || (e.type == Scribe::ChangeType::ComponentChanged && e.entityId == created.id);
	}
	check(r, sawMoved && sawChanged, "M4 move emits AssetMoved and ComponentChanged");
	check(r, scene->isDirty(), "M5 move marks the scene dirty");
	check(r, !session.canUndo() && !session.canRedo() && !session.undo() && !session.redo() &&
				 meshPath() == movedPath,
		  "M5 move clears stale asset paths from undo and redo");
	session.renameScene("V_MoveHistoryBarrier");
	check(r, session.undo() && scene->isDirty() && meshPath() == movedPath,
		  "M5 undo after a move preserves the dirty state");

	check(r, !session.moveAsset(imported.id, floorPath).ok(), "M6 move never overwrites");
	check(r, !session.moveAsset(imported.id, "res://validation_import/V_Move.wav").ok(),
		  "M6 move cannot change the asset type");
	check(r, session.moveAsset(imported.id, movedPath).status == Status::NoChange, "M6 move to the same path is NoChange");
	check(r, session.canRedo(), "M6 failed and unchanged moves preserve history");

	session.endMerge();
	const bool replaced = session.replaceReferences(movedPath, floorPath).status == Status::Applied && meshPath() == floorPath;
	session.undo();
	check(r, replaced, "R1 replaceReferences points the scene at another asset");
	check(r, meshPath() == movedPath, "R1 undo restores the old reference");

	check(r, !session.replaceReferences(movedPath, TEST_AUDIO).ok() && meshPath() == movedPath,
		  "R2 wrong asset type rejected");

	const bool cleared = session.replaceReferences(movedPath, "").status == Status::Applied && meshPath().empty() &&
				 !entity->has<CS::MeshRuntimeComponent>();
	session.undo();
	check(r, cleared, "R3 empty path clears references (Remove)");
	check(r, meshPath() == movedPath && entity->has<CS::MeshRuntimeComponent>(), "R3 undo brings the reference back");

	check(r, session.replaceReferences("res://validation_import/nothing.glb", floorPath).status == Status::NoChange,
		  "R4 no references is NoChange");

	session.endMerge();
	const bool empty = session.setField(created.id, "Mesh", "modelPath", std::string{}).ok();
	session.endMerge();
	const bool referenced = session.setField(created.id, "Mesh", "modelPath", std::string(movedPath)).ok();
	const bool undone = session.undo();
	const bool movedWithoutReferences = session.moveAsset(imported.id, startPath).status == Status::Applied;
	check(r, empty && referenced && undone && movedWithoutReferences && meshPath().empty() &&
				 !session.canUndo() && !session.canRedo(),
		  "M7 move clears paths held only in redo history");
	const bool saved = session.saveAs("res://.cthulhu/validation_asset_history.scene").ok();
	const bool movedClean = session.moveAsset(imported.id, movedPath).status == Status::Applied;
	session.renameScene("V_CleanMoveBarrier");
	check(r, saved && movedClean && session.undo() && !scene->isDirty(),
		  "M7 move without scene references preserves the clean save point");

	session.deleteEntity(created.id);
	assets.collectUnusedModels();

	const auto* mainRecord = project->getMainScene()
		? assets.getRegistry().findByPath(*project->getMainScene()) : nullptr;
	const Assets::AssetId mainId = mainRecord ? mainRecord->id : Assets::AssetId{};
	const auto mainMove = session.moveAsset(
		mainId, "res://validation_import/V_MainScene.scene");
	if (mainMove.status == Status::Applied)
	{
		check(r, session.moveAsset(mainId, *project->getMainScene()).ok(),
			  "M8 restore main scene after an unguarded move");
	}
	check(r, mainId.isValid() && mainMove.status == Status::Failed,
		  "M8 main scene cannot move without updating project settings");

	constexpr std::string_view sceneStart = "res://validation_import/V_Open.scene";
	constexpr std::string_view sceneMoved = "res://validation_import/moved/V_Open.scene";
	const bool sceneSaved = session.saveAs(sceneStart).ok();
	const bool scanned = assets.refreshRegistry().ok;
	const auto* document = assets.getRegistry().findByPath(sceneStart);
	const Assets::AssetId documentId =
		document ? document->id : Assets::AssetId{};
	const auto backingUsage = session.findAssetUsage(documentId);
	check(r, documentId.isValid() &&
				 std::any_of(
					 backingUsage.begin(), backingUsage.end(),
					 [&](const Scribe::SceneAssetUsage &entry) {
						 return entry.backingFile && entry.scenePath &&
								*entry.scenePath == sceneStart;
					 }),
		  "M8 usage identifies an open scene's backing file");

	check(r, documentId.isValid() && !session.deleteAsset(documentId).ok(),
		  "M8 an open scene's backing file cannot be deleted");
	const bool documentMoved = sceneSaved && scanned && documentId.isValid() &&
		session.moveAsset(documentId, sceneMoved).status == Status::Applied;

	const auto oldScene = project->resolveResourcePath(sceneStart);
	const auto newScene = project->resolveResourcePath(sceneMoved);
	check(r, documentMoved && scene->getResourcePath() &&
				 *scene->getResourcePath() == sceneMoved && session.save().ok() &&
				 oldScene && newScene && !std::filesystem::exists(*oldScene) &&
				 std::filesystem::exists(*newScene),
		  "M8 Save follows a moved open scene without recreating the old path");
	check(r, session.saveAs("res://.cthulhu/validation_asset_history.scene").ok(),
		  "M8 restore the validation document path");

	const auto previousMode = engine.getWorldMode();
	engine.setWorldMode(WorldMode::Edit);
	const auto parkedEntity = session.placeModel(imported.id, glm::vec3(0.0f));
	auto opened = session.openSceneDocument(SAVE_PATH);
	const auto parkedUsage = session.findAssetUsage(imported.id);
	check(r, opened.result.ok() && opened.previous &&
				 std::any_of(
					 parkedUsage.begin(), parkedUsage.end(),
					 [&](const Scribe::SceneAssetUsage &entry) {
						 return entry.scenePath ==
									opened.previous->snapshot.resourcePath &&
								std::any_of(
									entry.references.begin(),
									entry.references.end(),
									[&](const Scene::AssetReference &reference) {
										return reference.entity == parkedEntity.id &&
											   reference.component == "Mesh" &&
											   reference.field == "modelPath" &&
											   reference.path == movedPath;
									});
					 }),
		  "Inactive scene usage identifies the parked entity and field");
	const bool blocked = opened.result.ok() && !session.deleteAsset(imported.id).ok();
	const bool movedWhileParked = opened.result.ok() && session.moveAsset(imported.id, startPath).status == Status::Applied;
	const bool resumed = opened.previous && session.activateSceneDocument(opened.previous).result.ok();
	const auto *restored = engine.getActiveScene();
	const auto parkedMesh = resumed && restored ? restored->findEntity(parkedEntity.id) : std::nullopt;
	const auto *restoredMesh = parkedMesh ? parkedMesh->try_get<CS::MeshComponent>() : nullptr;
	check(r, parkedEntity.ok() && blocked && movedWhileParked && restoredMesh &&
				 restoredMesh->modelPath == startPath && restored->isDirty() && !session.canUndo(),
		  "Inactive scene protects its asset and follows a move");
	if (!opened.result.ok() || resumed)
		(void)session.deleteEntity(parkedEntity.id);
	engine.setWorldMode(previousMode);

	cleanImportFolder(engine);
}

inline void validateScribeDelete(Engine& engine, Results& r)
{
	using Status = Scribe::Result::Status;
	constexpr std::string_view floorPath = "res://assets/models/Floor.glb";
	constexpr std::string_view deletePath = "res://validation_import/V_Delete.glb";
	constexpr std::string_view gonePath = "res://validation_import/V_Gone.glb";

	const auto* project = engine.getProject();
	auto* scene = engine.getActiveScene();
	auto source = project ? project->resolveResourcePath(floorPath) : std::nullopt;
	if (!scene || !source)
	{
		check(r, false, "D0 delete test setup");
		return;
	}

	cleanImportFolder(engine);
	Scribe::Session session(engine);
	auto& assets = engine.getAssetManager();

	auto imported = session.importAsset(*source, deletePath);
	auto created = session.createEntity("V_DeleteUser");
	session.addComponent(created.id, "Mesh");
	session.setField(created.id, "Mesh", "modelPath", std::string(deletePath));
	auto file = project->resolveResourcePath(deletePath);
	if (!imported.ok() || !file)
	{
		check(r, false, "D0 delete test setup");
		session.deleteEntity(created.id);
		cleanImportFolder(engine);
		return;
	}

	const auto usage = session.findAssetUsage(imported.id);
	const bool listed = std::any_of(
		usage.begin(), usage.end(),
		[&](const Scribe::SceneAssetUsage &entry) {
			return entry.sceneName == scene->getName() &&
				   entry.scenePath == scene->getResourcePath() &&
				   std::any_of(
					   entry.references.begin(), entry.references.end(),
					   [&](const Scene::AssetReference &reference) {
						   return reference.entity == created.id &&
								  reference.component == "Mesh" &&
								  reference.field == "modelPath" &&
								  reference.path == deletePath;
					   });
		});

	check(r, listed, "D1 usage identifies the scene, entity and field");
	check(r, session.findAssetUsage({}).empty(),
		  "D1 unknown asset has no usage entries");
	check(r, !session.deleteAsset(imported.id).ok(), "D1 delete refused while the open scene uses the asset");
	check(r, std::filesystem::exists(*file) && assets.getRegistry().findById(imported.id),
		  "D1 refused delete changes nothing");
	check(r, session.canUndo(), "D1 refused delete preserves history");

	session.deleteEntity(created.id);
	(void) session.takeEvents();

	const bool deleted = session.deleteAsset(imported.id).status == Status::Applied;
	check(r, deleted && !std::filesystem::exists(*file), "D2 delete moves the file out of the project");
	check(r, deleted && !session.canUndo() && !session.canRedo() && !session.undo() && !session.redo(),
		  "D2 delete clears commands that could resurrect a deleted asset reference");

	Assets::AssetRegistry reloaded;
	const bool onDisk = reloaded.load(project->getRootPath() / ".cthulhu" / "assets.json") &&
						reloaded.findById(imported.id) != nullptr;
	check(r, !assets.getRegistry().findById(imported.id) && !onDisk, "D2 delete forgets the record (memory and disk)");

	bool sawDeleted = false;
	for (const auto& e : session.takeEvents())
	{
		sawDeleted = sawDeleted || (e.type == Scribe::ChangeType::AssetDeleted && e.assetId == imported.id);
	}
	check(r, sawDeleted, "D2 delete emits AssetDeleted");

	check(r, !session.deleteAsset(imported.id).ok(), "D3 deleting an unknown asset fails");

	auto gone = session.importAsset(*source, gonePath);
	auto goneFile = project->resolveResourcePath(gonePath);
	std::error_code error;
	if (goneFile)
	{
		std::filesystem::remove(*goneFile, error);
	}
	assets.refreshRegistry();
	check(r, gone.ok() && session.deleteAsset(gone.id).status == Status::Applied && !assets.getRegistry().findById(gone.id),
		  "D4 deleting a missing asset just forgets it");

	auto held = session.importAsset(*source, deletePath);
	auto handle = assets.acquireModel(deletePath);
	check(r, held.ok() && handle.isValid() && !session.deleteAsset(held.id).ok(),
		  "D5 delete refused while the asset is loaded");
	assets.releaseModel(handle);
	assets.collectUnusedModels();

    constexpr std::string_view failurePath =
        "res://validation_import/V_DeleteRegistryFailure.glb";
    const auto failureAsset = session.importAsset(*source, failurePath);
    const auto failureUser = session.createEntity("V_DeleteRegistryFailure");
    const bool historyReady =
        failureAsset.ok() && failureUser.ok() &&
        session.addComponent(failureUser.id, "Mesh").ok() &&
        session.setField(failureUser.id, "Mesh", "modelPath",
                         std::string(failurePath)).ok() &&
        session.deleteEntity(failureUser.id).ok() &&
        session.renameScene(scene->getName() + "_DeleteRegistryFailure").status ==
            Status::Applied &&
        session.undo() && session.canUndo() && session.canRedo();

    const auto realRegistry = project->getRootPath() / ".cthulhu" / "assets.json";
    const auto blockedRegistry = realRegistry.parent_path() /
        ("validation_delete_" + Assets::assetIdToString(failureAsset.id) + ".json");

    // Block replacement of a copied registry, leaving the real file intact.
    std::error_code failureError;
    const bool copied = historyReady &&
        std::filesystem::copy_file(realRegistry, blockedRegistry, failureError);
    bool blocked = false;
    if (copied && assets.getRegistry().load(blockedRegistry))
    {
        blocked = std::filesystem::remove(blockedRegistry, failureError) &&
                  std::filesystem::create_directory(blockedRegistry, failureError);
    }
    check(r, blocked, "D6 registry save-failure setup");

    if (blocked)
    {
        (void)session.takeEvents();
        const auto result = session.deleteAsset(failureAsset.id);
        const auto *remaining = assets.getRegistry().findById(failureAsset.id);
        const auto deletedFile = project->resolveResourcePath(failurePath);
        const bool fileGone = deletedFile &&
            !std::filesystem::exists(*deletedFile, failureError) && !failureError;

        check(r, !result.ok() && fileGone && remaining && remaining->missing,
              "D6 failed registry save retains the trashed asset as missing");
        check(r, !session.canUndo() && !session.canRedo() &&
                     !session.undo() && !session.redo(),
              "D6 partial delete clears stale undo and redo paths");

        bool notified = false;
        for (const auto &change : session.takeEvents())
        {
            notified = notified ||
                (change.type == Scribe::ChangeType::AssetRegistryRefreshed &&
                 change.assetId == failureAsset.id);
        }
        check(r, notified, "D6 partial delete notifies asset views");
    }

    if (copied)
    {
        check(r, assets.getRegistry().load(realRegistry),
              "D6 restores the real registry binding");
        std::filesystem::remove(blockedRegistry, failureError);
        check(r, !failureError, "D6 removes the registry blocker");
        std::filesystem::remove(blockedRegistry.string() + ".tmp", failureError);
        check(r, !failureError, "D6 removes the registry temporary file");
    }

    if (failureUser.ok() && scene->isEntityAlive(failureUser.id))
    {
        session.deleteEntity(failureUser.id);
    }

	cleanImportFolder(engine);
}

inline void validateMissingAssets(Engine& engine, Results& r)
{
	using Status = Scribe::Result::Status;
	constexpr std::string_view floorPath = "res://assets/models/Floor.glb";
	constexpr std::string_view missingPath = "res://validation_import/V_NotThere.glb";

	Scribe::Session session(engine);

	auto groupFor = [&](std::string_view path) -> std::optional<Scribe::MissingAsset> {
		for (auto& missing : session.findMissingAssets())
		{
			if (missing.path == path)
			{
				return missing;
			}
		}
		return std::nullopt;
	};

	auto a = session.createEntity("V_MissingA");
	auto b = session.createEntity("V_MissingB");
	for (const auto id : {a.id, b.id})
	{
		session.addComponent(id, "Mesh");
		session.setField(id, "Mesh", "modelPath", std::string(missingPath));
	}

	auto group = groupFor(missingPath);
	check(r, group.has_value(), "Q1 missing asset is reported");
	check(r, group && group->references.size() == 2, "Q1 references are grouped per missing file");
	check(r, !groupFor(floorPath), "Q1 existing assets are not reported");

	session.endMerge();
	const bool relocated = session.replaceReferences(missingPath, floorPath).status == Status::Applied &&
						   !groupFor(missingPath);
	session.undo();
	check(r, relocated, "Q2 Relocate fixes the missing asset");
	check(r, groupFor(missingPath).has_value(), "Q2 undo Relocate reports it again");

	check(r, session.replaceReferences(missingPath, "").status == Status::Applied && !groupFor(missingPath),
		  "Q3 Remove clears the missing asset");

	session.deleteEntity(a.id);
	session.deleteEntity(b.id);
}


inline void validateReimport(Engine& engine, Results& r)
{
	using Status = Scribe::Result::Status;
	constexpr std::string_view clipPath =
		"res://validation_import/caf\xC3\xA9/V_Reimport.wav";

	const auto* project = engine.getProject();
	auto* scene = engine.getActiveScene();
	auto source = project ? project->resolveResourcePath(TEST_AUDIO) : std::nullopt;
	if (!scene || !source || !std::filesystem::exists(*source))
	{
		Log::Print("SKIP  E reimport checks (set TEST_AUDIO to a real file)", "Validation", LogType::LOG_WARNING);
		return;
	}

	cleanImportFolder(engine);
	Scribe::Session session(engine);
	auto& assets = engine.getAssetManager();

	auto imported = session.importAsset(*source, clipPath);
	flecs::entity e = scene->createEntity("V_Reimport");
	e.set(CS::AudioSourceComponent{std::string(clipPath), 0.0f, true}); // silent, looping
	if (!imported.ok() || !e.has<CS::AudioSourceRuntimeComponent>())
	{
		check(r, false, "E0 reimport test setup");
		scene->destroyEntity(idOf(e));
		cleanImportFolder(engine);
		return;
	}

	auto play = [&]() {
		e.get_mut<CS::AudioSourceRuntimeComponent>().playRequested = true;
		scene->getWorld().progress(0.0f);
	};

	const size_t base = sounds();
	play();
	const auto handle = e.get<CS::AudioSourceRuntimeComponent>().clip;
	const uint32_t refsBefore = assets.getAudioClipRefCount(handle);
	(void) session.takeEvents();

	Assets::AudioImportSettings streamed;
	streamed.stream = true;
	check(r, session.setAudioImportSettings(imported.id, streamed).status == Status::Applied,
		  "E1 import settings change applies");
	check(r, Core::Audio::isClipStreamed(assets.getAudioClip(handle)) && assets.getAudioClipRefCount(handle) == refsBefore,
		  "E1 loaded clip is rebuilt in place (same handle)");
	check(r, sounds() == base, "E2 sounds of the old clip are stopped");

	Assets::AssetRegistry reloaded;
	const auto* onDisk = reloaded.load(project->getRootPath() / ".cthulhu" / "assets.json")
							 ? reloaded.findById(imported.id) : nullptr;
	check(r, onDisk && onDisk->audio.stream, "E3 new settings are saved to the registry file");

	check(r, session.setAudioImportSettings(imported.id, streamed).status == Status::NoChange,
		  "E4 same settings is NoChange");

	bool sawChanged = false;
	for (const auto& ev : session.takeEvents())
	{
		sawChanged = sawChanged || (ev.type == Scribe::ChangeType::AssetSettingsChanged && ev.assetId == imported.id);
	}
	check(r, sawChanged, "E5 emits AssetSettingsChanged");

	const auto* floor = assets.getRegistry().findByPath("res://assets/models/Floor.glb");
	check(r, floor && !session.setAudioImportSettings(floor->id, streamed).ok(), "E6 non-audio asset rejected");

	play();
	check(r, sounds() == base + 1, "E7 the rebuilt clip plays");

	Core::Audio::stopClipSounds(assets.getAudioClip(handle));
	constexpr std::string_view movedClip =
		"res://validation_import/moved_caf\xC3\xA9/V_Reimport.wav";
	const auto moved = session.moveAsset(imported.id, movedClip);
	check(r, moved.status == Status::Applied &&
				 e.get<CS::AudioSourceComponent>().filePath == movedClip &&
				 e.get<CS::AudioSourceRuntimeComponent>().clip == handle &&
				 assets.getAudioClipRefCount(handle) == refsBefore,
		  "E8 streamed clip move preserves its handle and scene reference");

	play();
	check(r, sounds() == base + 1 &&
				 Core::Audio::isSoundActive(
					 e.get<CS::AudioSourceRuntimeComponent>().soundInstanceId),
		  "E8 moved streamed clip opens its new source path");

	scene->destroyEntity(idOf(e));
	assets.collectUnusedAudioClips();
	cleanImportFolder(engine);
}

} // namespace Cthulhu::Validation
