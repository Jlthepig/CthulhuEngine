#pragma once

#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline void validateAssetRegistry(Engine& engine, Results& r)
{
    auto& registry = engine.getAssetManager().getRegistry();
    const auto* project = engine.getProject();
    const auto root = project->getRootPath();

    const auto* floor = registry.findByPath("res://assets/models/Floor.glb");
    check(r, floor && floor->id.isValid() &&
             registry.findByPath("res://assets/models/DamagedHelmet.glb") &&
             registry.findByPath(TEST_AUDIO) &&
             registry.findByPath("res://assets/scenes/test.scene"),
          "J1 project assets are registered with IDs");
    if (!floor)
    {
        return;
    }
    const Assets::AssetId floorId = floor->id; // records may move on rescan; keep the ID, not the pointer

    check(r, registry.findByPath("res://.cthulhu/validation.scene") == nullptr,
          "J5 .cthulhu contents are never registered");

    Assets::AssetRegistry reloaded;
    const auto* again = reloaded.load(root / ".cthulhu" / "assets.json")
                            ? reloaded.findByPath("res://assets/models/Floor.glb") : nullptr;
    check(r, again && again->id == floorId, "J2 IDs persist on disk");

    const auto rescan = registry.scan(*project);
    const auto* floorAfter = registry.findByPath("res://assets/models/Floor.glb");
    check(r, rescan.ok && rescan.added == 0 && floorAfter && floorAfter->id == floorId,
          "J3 rescan keeps IDs and adds nothing");

    const auto tempDir = root / "validation_tmp";
    std::error_code error;
    std::filesystem::create_directories(tempDir, error);
    std::filesystem::copy_file(root / "assets/models/Floor.glb", tempDir / "V_Copy.glb",
                               std::filesystem::copy_options::overwrite_existing, error);

    const auto added = registry.scan(*project);
    const auto* copy = registry.findByPath("res://validation_tmp/V_Copy.glb");
    const Assets::AssetId copyId = copy ? copy->id : Assets::AssetId{};
    check(r, !error && added.added == 1 && copy && !copy->missing && copyId != floorId,
          "J4 new file gets a new ID");

    std::filesystem::remove_all(tempDir, error);
    registry.scan(*project);
    const auto* gone = registry.findById(copyId);
    check(r, gone && gone->missing, "J4 deleted file is flagged missing but keeps its ID");
    check(r, registry.forget(copyId) && !registry.findById(copyId), "J4 forgetting a missing asset removes it");

    const auto badFile = root / ".cthulhu" / "validation_badregistry.json";
    {
        std::ofstream out(badFile, std::ios::trunc);
        out << "{ this is not json";
    }

    Assets::AssetRegistry corrupt;
    const bool loadedBad = corrupt.load(badFile);
    const auto badScan = corrupt.scan(*project);
    const bool savedBad = corrupt.save();
    const std::string content = Utils::FileReader::readFile(badFile.string());
    check(r, !loadedBad && !badScan.ok && !savedBad && content == "{ this is not json",
          "J6 corrupt registry is never overwritten");
}

inline void validateAssetManager(Engine& engine, Results& r)
{
	auto& assets = engine.getAssetManager();
	auto a = assets.loadModel("res://assets/models/Floor.glb");
	const size_t afterFirst = assets.getLoadedModelCount();
	auto b = assets.loadModel("res://assets/models/../models/Floor.glb");
	check(r, a.isValid() && a == b, "A1 same model via different paths -> same handle");
	check(r, assets.getLoadedModelCount() == afterFirst, "A1 repeat load does not load again");
	check(r, assets.getModel(a) && !assets.getModel(a)->meshes.empty(), "A1 handle resolves to model");
	auto missing = assets.loadModel("res://assets/models/missing.glb");
	check(r, !missing.isValid() && !assets.getModel(missing), "A2 missing model -> invalid handle");
	check(r, assets.getLoadedModelCount() == afterFirst, "A2 failed load caches nothing");
	auto stale = a;
	stale.generation += 1;
	check(r, !assets.getModel(stale), "A3 stale generation -> nullptr");
	auto escape = assets.loadModel("res://../outside.glb");
	check(r, !escape.isValid(), "A4 root-escaping path rejected");
}

inline void validateAssetTypes(Engine& engine, Results& r)
{
	using Assets::AssetType;
	using Assets::getAssetType;
	check(r, getAssetType("res://models/Gun.GLB") == AssetType::Model, "G1 .glb (any case) -> Model");
	check(r, getAssetType("res://audio/shot.wav") == AssetType::Audio, "G1 .wav -> Audio");
	check(r, getAssetType("res://maps/level.scene") == AssetType::Scene, "G1 .scene -> Scene");
	check(r, getAssetType("res://folder.v2/readme") == AssetType::Unknown, "G1 dot in directory name -> Unknown");
	check(r, getAssetType("res://noextension") == AssetType::Unknown, "G1 no extension -> Unknown");
	auto& assets = engine.getAssetManager();
	const size_t models = assets.getLoadedModelCount();
	auto wrong = assets.loadModel("res://assets/audio/gunshot.wav");
	check(r, !wrong.isValid() && assets.getLoadedModelCount() == models, "G2 loadModel rejects a non-model asset");
	const auto* before = engine.getActiveScene();
	check(r, !engine.loadScene("res://assets/models/Floor.glb") && engine.getActiveScene() == before,
		  "G3 loadScene rejects a non-scene file");
}

inline void validateMeshLifecycle(Engine& engine, CS::Scene& scene, Results& r)
{
	auto& assets = engine.getAssetManager();
	flecs::entity m = scene.createEntity("V_Mesh");
	m.set(CS::MeshComponent{"res://assets/models/Floor.glb"});
	const auto* rt = m.try_get<CS::MeshRuntimeComponent>();
	const Assets::ModelHandle floorHandle = rt ? rt->model : Assets::ModelHandle{};
	check(r, rt && assets.getModel(rt->model), "B1 add MeshComponent -> runtime model");
	m.set(CS::MeshComponent{"res://assets/models/DamagedHelmet.glb"});
	rt = m.try_get<CS::MeshRuntimeComponent>();
	check(r, rt && rt->model.isValid() && !(rt->model == floorHandle), "B2 change modelPath -> runtime rebuilt");
	m.set(CS::MeshComponent{"res://assets/models/missing.glb"});
	check(r, m.has<CS::MeshComponent>() && !m.has<CS::MeshRuntimeComponent>(), "B3 bad modelPath -> authoring kept, no runtime");
	m.set(CS::MeshComponent{"res://assets/models/Floor.glb"});
	m.remove<CS::MeshComponent>();
	check(r, !m.has<CS::MeshRuntimeComponent>(), "B4 remove MeshComponent -> runtime removed");
	scene.destroyEntity(idOf(m));
}

inline void validateRefCounting(Engine& engine, CS::Scene& scene, Results& r)
{
	auto& assets = engine.getAssetManager();
	const Assets::ModelHandle floor = assets.loadModel("res://assets/models/Floor.glb");
	const Assets::ModelHandle helmet = assets.loadModel("res://assets/models/DamagedHelmet.glb");
	const uint32_t floorBase = assets.getModelRefCount(floor);
	const uint32_t helmetBase = assets.getModelRefCount(helmet);
	flecs::entity a = scene.createEntity("V_RefA");
	flecs::entity b = scene.createEntity("V_RefB");
	a.set(CS::MeshComponent{"res://assets/models/Floor.glb"});
	b.set(CS::MeshComponent{"res://assets/models/Floor.glb"});
	check(r, assets.getModelRefCount(floor) == floorBase + 2, "C2 two users -> two references");
	a.set(CS::MeshComponent{"res://assets/models/Floor.glb"});
	check(r, assets.getModelRefCount(floor) == floorBase + 2, "C2 re-setting same path keeps count stable");
	a.set(CS::MeshComponent{"res://assets/models/DamagedHelmet.glb"});
	check(r, assets.getModelRefCount(floor) == floorBase + 1 &&
			 assets.getModelRefCount(helmet) == helmetBase + 1, "C2 changing modelPath moves the reference");
	a.set(CS::MeshComponent{"res://assets/models/missing.glb"});
	check(r, assets.getModelRefCount(helmet) == helmetBase, "C2 bad modelPath releases old reference");
	b.remove<CS::MeshComponent>();
	check(r, assets.getModelRefCount(floor) == floorBase, "C2 remove MeshComponent releases reference");
	b.set(CS::MeshComponent{"res://assets/models/Floor.glb"});
	scene.destroyEntity(idOf(b));
	check(r, assets.getModelRefCount(floor) == floorBase, "C2 destroy entity releases reference");
	scene.destroyEntity(idOf(a));
}

inline void validateAudioClips(Engine& engine, CS::Scene& scene, Results& r)
{
	const auto* project = engine.getProject();
	auto resolved = project ? project->resolveResourcePath(TEST_AUDIO) : std::optional<std::filesystem::path>{};
	if (!resolved || !std::filesystem::exists(*resolved))
	{
		Log::Print("SKIP  H audio clip checks (set TEST_AUDIO to a real file)", "Validation", LogType::LOG_WARNING);
		return;
	}
	auto& assets = engine.getAssetManager();
	const std::string path(TEST_AUDIO);
	const Assets::AudioClipHandle clip = assets.loadAudioClip(TEST_AUDIO);
	const uint32_t base = assets.getAudioClipRefCount(clip);
	const size_t loadedClips = assets.getLoadedAudioClipCount();
	const size_t baseSounds = sounds();
	flecs::entity a = scene.createEntity("V_ClipA");
	flecs::entity b = scene.createEntity("V_ClipB");
	a.set(CS::AudioSourceComponent{path, 0.0f, true});
	b.set(CS::AudioSourceComponent{path, 0.0f, true});
	const auto* ra = a.try_get<CS::AudioSourceRuntimeComponent>();
	const auto* rb = b.try_get<CS::AudioSourceRuntimeComponent>();
	check(r, ra && rb && ra->clip == rb->clip && assets.getAudioClipRefCount(clip) == base + 2 &&
			 assets.getLoadedAudioClipCount() == loadedClips, "H1 two sources share one clip");
	a.get_mut<CS::AudioSourceRuntimeComponent>().playRequested = true;
	b.get_mut<CS::AudioSourceRuntimeComponent>().playRequested = true;
	scene.getWorld().progress(0.0f);
	check(r, sounds() == baseSounds + 2 && assets.getLoadedAudioClipCount() == loadedClips,
		  "H2 playing does not reload the clip");
	a.set(CS::AudioSourceComponent{path, 0.5f, true});
	ra = a.try_get<CS::AudioSourceRuntimeComponent>();
	check(r, ra && ra->isPlaying && sounds() == baseSounds + 2, "H3 editing a same-clip source keeps playback");
	a.set(CS::AudioSourceComponent{"res://assets/models/Floor.glb", 0.5f, true});
	check(r, !a.has<CS::AudioSourceRuntimeComponent>() && assets.getAudioClipRefCount(clip) == base + 1 &&
			 sounds() == baseSounds + 1, "H4 wrong-type path -> old sound stopped, clip released");
	scene.destroyEntity(idOf(a));
	scene.destroyEntity(idOf(b));
	check(r, assets.getAudioClipRefCount(clip) == base && sounds() == baseSounds,
		  "H5 destroying sources releases the clip and stops sounds");
}

inline void validateUnusedCollection(Engine& engine, Results& r)
{
	auto& assets = engine.getAssetManager();
	constexpr std::string_view floorOnlyPath = "res://.cthulhu/validation_flooronly.scene";
	auto resolved = engine.getProject()->resolveResourcePath(floorOnlyPath);
	if (!resolved)
	{
		check(r, false, "D0 resolve floor-only scene path");
		return;
	}
	{
		std::ofstream out(*resolved, std::ios::trunc);
		out << R"({
	"format_version": 2,
	"name": "validation_flooronly",
	"entities": [
		{ "id": "aaaabbbbccccddddeeeeffff00001111", "name": "FloorOnly",
		  "model": "res://assets/models/Floor.glb",
		  "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1] }
	],
	"directional_light": { "direction": [0,-1,0], "color": [1,1,1], "intensity": 1 },
	"point_lights": []
})";
	}
	const Assets::ModelHandle floorBefore = assets.loadModel("res://assets/models/Floor.glb");
	const Assets::ModelHandle helmetBefore = assets.loadModel("res://assets/models/DamagedHelmet.glb");
	const Assets::AudioClipHandle clipBefore = assets.loadAudioClip(TEST_AUDIO);
	const bool loaded = engine.loadScene(floorOnlyPath);
	check(r, loaded, "D0 floor-only scene loads");
	check(r, !assets.getAudioClip(clipBefore) && assets.getLoadedAudioClipCount() == 0,
		  "I2 switching to a scene without audio frees the clip");
	check(r, !assets.getModel(helmetBefore), "D1 switching to a scene without the helmet unloads it");
	check(r, assets.getModel(floorBefore) != nullptr, "D1 shared model survives the switch");
	check(r, assets.loadModel("res://assets/models/Floor.glb") == floorBefore, "D1 shared model was not reloaded");
	const bool back = engine.loadScene(SAVE_PATH);
	const Assets::ModelHandle helmetAfter = assets.loadModel("res://assets/models/DamagedHelmet.glb");
	check(r, back && assets.getModel(helmetAfter) != nullptr, "D2 unloaded model reloads when needed again");
	check(r, helmetAfter.index == helmetBefore.index && helmetAfter.generation != helmetBefore.generation,
		  "E1 freed slot is reused with a new generation");
	check(r, !assets.getModel(helmetBefore), "E2 old handle does not resolve to the slot's new occupant");
	const uint32_t refs = assets.getModelRefCount(helmetAfter);
	assets.releaseModel(helmetBefore);
	check(r, assets.getModelRefCount(helmetAfter) == refs, "E3 releasing a stale handle cannot affect the new occupant");
	const Assets::AudioClipHandle clipAfter = assets.loadAudioClip(TEST_AUDIO);
	check(r, assets.getAudioClip(clipAfter) && !assets.getAudioClip(clipBefore) &&
			 clipAfter.index == clipBefore.index && clipAfter.generation != clipBefore.generation,
		  "I3 clip reloads into its old slot with a new generation; stale handle -> nullptr");
}

inline void validateBadModelInScene(Engine& engine, Results& r)
{
	constexpr std::string_view path = "res://.cthulhu/validation_badmodel.scene";
	auto resolved = engine.getProject()->resolveResourcePath(path);
	if (!resolved)
	{
		check(r, false, "B5 resolve bad-model scene path");
		return;
	}
	{
		std::ofstream out(*resolved, std::ios::trunc);
		out << R"({
	"format_version": 2,
	"name": "validation_badmodel",
	"entities": [
		{ "id": "11112222333344445555666677778888", "name": "BadModel",
		  "model": "res://assets/models/missing.glb",
		  "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1],
		  "physics": { "type": "static", "half_extent": [1,1,1] } }
	],
	"directional_light": { "direction": [0,-1,0], "color": [1,1,1], "intensity": 1 },
	"point_lights": []
})";
	}
	const bool loaded = engine.loadScene(path);
	check(r, loaded, "B5 scene with missing model still loads");
	if (!loaded)
	{
		return;
	}
	bool ok = false;
	engine.getActiveScene()->getWorld().each([&](flecs::entity e, const CS::NameComponent& name)
	{
		if (name.name == "BadModel")
		{
			ok = e.has<CS::MeshComponent>() && !e.has<CS::MeshRuntimeComponent>() &&
				 e.has<CS::PhysicsRuntimeComponent>() && e.has<CS::TagActive>();
		}
	});
	check(r, ok, "B5 missing model keeps mesh authoring and all other components");
}
} // namespace Cthulhu::Validation
