#pragma once

#include <vector>

#include "session.hpp"
#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline constexpr std::string_view IMPORT_DIR = "res://validation_import";
inline constexpr std::string_view IMPORT_PATH = "res://validation_import/V_Imported.wav";

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

	auto imported = session.importAsset(*source, IMPORT_PATH);
	const auto* record = imported.ok() ? assets.getRegistry().findById(imported.id) : nullptr;
	auto target = project->resolveResourcePath(IMPORT_PATH);
	check(r, imported.ok() && imported.id.isValid() && target && std::filesystem::exists(*target),
		  "I1 import copies the file into the project");
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
	constexpr std::string_view startPath = "res://validation_import/V_Move.glb";
	constexpr std::string_view movedPath = "res://validation_import/moved/V_Moved.glb";

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

	check(r, !session.moveAsset(imported.id, floorPath).ok(), "M6 move never overwrites");
	check(r, !session.moveAsset(imported.id, "res://validation_import/V_Move.wav").ok(),
		  "M6 move cannot change the asset type");
	check(r, session.moveAsset(imported.id, movedPath).status == Status::NoChange, "M6 move to the same path is NoChange");

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

	session.deleteEntity(created.id);
	assets.collectUnusedModels();
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

	check(r, !session.deleteAsset(imported.id).ok(), "D1 delete refused while the open scene uses the asset");
	check(r, std::filesystem::exists(*file) && assets.getRegistry().findById(imported.id),
		  "D1 refused delete changes nothing");

	session.deleteEntity(created.id);
	(void) session.takeEvents();

	const bool deleted = session.deleteAsset(imported.id).status == Status::Applied;
	check(r, deleted && !std::filesystem::exists(*file), "D2 delete moves the file out of the project");

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

	cleanImportFolder(engine);
}

} // namespace Cthulhu::Validation