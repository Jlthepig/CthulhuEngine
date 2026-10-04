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
} // namespace Cthulhu::Validation