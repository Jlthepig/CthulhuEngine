#pragma once

// TEMPORARY: Phase 2 Step 5.5 component-architecture validation.
// Delete this file and the --validate hook in main.cpp once 5.5 passes.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#include "audio.hpp"
#include "characterController.hpp"
#include "components.hpp"
#include "engine.hpp"
#include "fileReader.hpp"
#include "scene.hpp"
#include "log_utils.hpp"

namespace Cthulhu::Validation
{
namespace CS = Cthulhu::Scene;
using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

// Point at any real audio file in the Sandbox. Audio checks are skipped if missing.
inline constexpr std::string_view TEST_AUDIO = "res://assets/audio/gunshot.wav";

inline constexpr std::string_view SAVE_PATH = "res://.cthulhu/validation.scene";
inline constexpr std::string_view BROKEN_PATH = "res://.cthulhu/validation_broken.scene";

struct Results
{
    int passed = 0;
    int failed = 0;
};

inline void check(Results& r, bool condition, const std::string& name)
{
    if (condition)
    {
        ++r.passed;
        Log::Print("PASS  " + name, "Validation", LogType::LOG_SUCCESS);
    }
    else
    {
        ++r.failed;
        Log::Print("FAIL  " + name, "Validation", LogType::LOG_ERROR);
    }
}

inline CS::EntityId idOf(flecs::entity e)
{
    const auto* identity = e.try_get<CS::EntityIdentityComponent>();
    return identity ? identity->id : CS::EntityId{};
}

inline uint32_t bodies(Engine& engine) { return engine.getPhysicsWorld().getBodyCount(); }
inline int characters() { return Physics::CharacterController::getLiveCharacterCount(); }
inline size_t sounds() { return Core::Audio::getActiveSoundCount(); }

inline void checkRefInvariant(Engine& engine, Results& r, const std::string& label)
{
    auto* scene = engine.getActiveScene();
    const int runtimes = scene ? scene->getWorld().count<CS::MeshRuntimeComponent>() : 0;

    check(r, engine.getAssetManager().getTotalModelRefCount() == static_cast<uint32_t>(runtimes),
          "F1 model refs == mesh runtimes (" + label + ")");
}

// ---------------------------------------------------------------- 1-4 physics
inline void validatePhysics(Engine& engine, CS::Scene& scene, Results& r)
{
    const uint32_t base = bodies(engine);

    flecs::entity e = scene.createEntity("V_Physics");
    e.set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.5f), 1.0f});

    const auto* rt = e.try_get<CS::PhysicsRuntimeComponent>();
    check(r, rt && rt->bodyId != 0, "01 add PhysicsComponent -> runtime body exists");
    check(r, bodies(engine) == base + 1, "01 add PhysicsComponent -> exactly one Jolt body");

    const uint32_t oldBody = rt ? rt->bodyId : 0;
    e.set(CS::PhysicsComponent{CS::PhysicsBodyType::Static, glm::vec3(1.0f), 1.0f});
    rt = e.try_get<CS::PhysicsRuntimeComponent>();
    check(r, rt && rt->bodyId != 0 && rt->bodyId != oldBody, "02 modify PhysicsComponent -> body rebuilt");
    check(r, bodies(engine) == base + 1, "02 modify PhysicsComponent -> old body destroyed");

    e.remove<CS::PhysicsComponent>();
    check(r, !e.has<CS::PhysicsRuntimeComponent>(), "03 remove PhysicsComponent -> runtime removed");
    check(r, bodies(engine) == base, "03 remove PhysicsComponent -> Jolt body destroyed");

    flecs::entity parent = scene.createEntity("V_PhysicsParent");
    flecs::entity child = scene.createEntity("V_PhysicsChild");
    parent.set(CS::PhysicsComponent{});
    child.set(CS::PhysicsComponent{});
    scene.setParent(idOf(child), idOf(parent));
    scene.destroyEntity(idOf(parent));
    check(r, bodies(engine) == base, "04 delete physics subtree -> no leaked bodies");

    scene.destroyEntity(idOf(e));
}

// ---------------------------------------------------------------- 5-7 character
inline void validateCharacter(CS::Scene& scene, Results& r)
{
    const int base = characters();

    flecs::entity c = scene.createEntity("V_Character");
    c.set(CS::CharacterControllerComponent{});
    const auto* rt = c.try_get<CS::CharacterControllerRuntimeComponent>();
    check(r, rt && rt->character, "05 add CharacterController -> CharacterVirtual created");
    check(r, characters() == base + 1, "05 add CharacterController -> exactly one CharacterVirtual");

    CS::CharacterControllerComponent config{};
    config.capsuleRadius = 0.5f;
    config.capsuleHeight = 1.5f;
    c.set(config);
    rt = c.try_get<CS::CharacterControllerRuntimeComponent>();
    check(r, rt && rt->character, "06 modify controller -> runtime rebuilt");
    check(r, characters() == base + 1, "06 modify controller -> old CharacterVirtual destroyed once");

    c.remove<CS::CharacterControllerComponent>();
    check(r, !c.has<CS::CharacterControllerRuntimeComponent>(), "05 remove controller -> runtime removed");
    check(r, characters() == base, "05 remove controller -> CharacterVirtual destroyed");

    flecs::entity c2 = scene.createEntity("V_Character2");
    c2.set(CS::CharacterControllerComponent{});
    scene.destroyEntity(idOf(c2));
    check(r, characters() == base, "07 delete character entity -> CharacterVirtual destroyed");

    scene.destroyEntity(idOf(c));
}

// ---------------------------------------------------------------- 8 weapon
inline void validateWeapon(CS::Scene& scene, Results& r)
{
    flecs::entity w = scene.createEntity("V_Weapon");
    w.set(CS::WeaponComponent{5.0f, 50.0f});
    check(r, w.has<CS::WeaponRuntimeComponent>(), "08 add WeaponComponent -> runtime created");

    if (w.has<CS::WeaponRuntimeComponent>())
    {
        auto& rt = w.get_mut<CS::WeaponRuntimeComponent>();
        rt.timeSinceLastShot = 42.0f;
        rt.wantsToFire = true;
    }

    auto dupId = scene.duplicateEntity(idOf(w));
    check(r, dupId.has_value(), "08 duplicate weapon entity");

    if (dupId)
    {
        auto dup = scene.findEntity(*dupId);
        const auto* cfg = dup ? dup->try_get<CS::WeaponComponent>() : nullptr;
        const auto* rt = dup ? dup->try_get<CS::WeaponRuntimeComponent>() : nullptr;
        check(r, cfg && cfg->fireRate == 5.0f && cfg->maxRange == 50.0f, "08 duplicate copies weapon authoring data");
        check(r, rt && rt->timeSinceLastShot == 0.0f && !rt->wantsToFire, "08 duplicate has fresh weapon runtime");
        scene.destroyEntity(*dupId);
    }

    scene.destroyEntity(idOf(w));
}

// ---------------------------------------------------------------- 9 audio
inline void validateAudio(Engine& engine, CS::Scene& scene, Results& r)
{
    const auto* project = engine.getProject();
    std::optional<std::filesystem::path> resolved;
    if (project)
    {
        resolved = project->resolveResourcePath(TEST_AUDIO);
    }

    if (!resolved || !std::filesystem::exists(*resolved))
    {
        Log::Print("SKIP  09 audio checks (set TEST_AUDIO to a real file)", "Validation", LogType::LOG_WARNING);
        return;
    }

    const size_t base = sounds();

    auto play = [&](flecs::entity e)
    {
        e.get_mut<CS::AudioSourceRuntimeComponent>().playRequested = true;
        scene.getWorld().progress(0.0f);
    };

    // volume 0 + loop so it stays "playing" silently
    flecs::entity a = scene.createEntity("V_Audio");
    a.set(CS::AudioSourceComponent{std::string(TEST_AUDIO), 0.0f, true});
    play(a);

    const auto* rt = a.try_get<CS::AudioSourceRuntimeComponent>();
    check(r, rt && rt->isPlaying && rt->soundInstanceId != 0, "09 play request -> sound playing");
    check(r, sounds() == base + 1, "09 play request -> one active sound");

    a.remove<CS::AudioSourceComponent>();
    check(r, !a.has<CS::AudioSourceRuntimeComponent>(), "09 remove AudioSourceComponent -> runtime removed");
    check(r, sounds() == base, "09 remove AudioSourceComponent -> sound stopped");

    flecs::entity a2 = scene.createEntity("V_Audio2");
    a2.set(CS::AudioSourceComponent{std::string(TEST_AUDIO), 0.0f, true});
    play(a2);
    scene.destroyEntity(idOf(a2));
    check(r, sounds() == base, "09 delete audio entity -> sound stopped");

    scene.destroyEntity(idOf(a));
}

// ---------------------------------------------------------------- 10-11 mesh + duplication
inline void validateMeshAndDuplication(Engine& engine, CS::Scene& scene, Results& r)
{
    std::optional<flecs::entity> meshEntity;
    bool allValid = true;
    int meshCount = 0;

    scene.getWorld().each([&](flecs::entity e, const CS::MeshComponent&)
    {
        ++meshCount;
        const auto* rt = e.try_get<CS::MeshRuntimeComponent>();
        const auto* model = rt ? engine.getAssetManager().getModel(rt->model) : nullptr;
        if (!model || model->meshes.empty())
        {
            allValid = false;
        }
        if (!meshEntity)
        {
            meshEntity = e;
        }
    });

    check(r, meshCount > 0, "10 scene contains mesh entities");
    check(r, allValid, "10 every MeshComponent has a valid runtime model");

    if (meshEntity)
    {
        const CS::EntityId srcId = idOf(*meshEntity);
        auto dupId = scene.duplicateEntity(srcId);
        check(r, dupId && *dupId != srcId, "11 duplicate mesh entity -> new UUID");

        if (dupId)
        {
            auto dup = scene.findEntity(*dupId);
            const auto* srcMesh = meshEntity->try_get<CS::MeshComponent>();
            const auto* dupMesh = dup ? dup->try_get<CS::MeshComponent>() : nullptr;
            const auto* dupRt = dup ? dup->try_get<CS::MeshRuntimeComponent>() : nullptr;
            check(r, srcMesh && dupMesh && srcMesh->modelPath == dupMesh->modelPath, "11 duplicate copies mesh authoring data");
            check(r, dupRt && engine.getAssetManager().getModel(dupRt->model), "11 duplicate has a runtime model");
            check(r, dup && dup->has<CS::TagActive>(), "11 duplicate is active/renderable");
            scene.destroyEntity(*dupId);
        }
    }

    const uint32_t base = bodies(engine);
    flecs::entity p = scene.createEntity("V_DupParent");
    flecs::entity c = scene.createEntity("V_DupChild");
    p.set(CS::PhysicsComponent{});
    c.set(CS::PhysicsComponent{});
    scene.setParent(idOf(c), idOf(p));

    auto dupRoot = scene.duplicateEntity(idOf(p));
    check(r, dupRoot && *dupRoot != idOf(p), "11 duplicate subtree -> new root UUID");

    if (dupRoot)
    {
        auto kids = scene.getChildren(*dupRoot);
        check(r, kids.size() == 1 && kids[0] != idOf(c), "11 duplicate subtree -> child copied with new UUID");
        check(r, bodies(engine) == base + 4, "11 duplicate subtree -> fresh body for every copy");

        auto dupParent = scene.findEntity(*dupRoot);
        const auto* srcRt = p.try_get<CS::PhysicsRuntimeComponent>();
        const auto* dupRt = dupParent ? dupParent->try_get<CS::PhysicsRuntimeComponent>() : nullptr;
        check(r, srcRt && dupRt && srcRt->bodyId != dupRt->bodyId, "11 duplicate does not share runtime body");

        scene.destroyEntity(*dupRoot);
    }

    scene.destroyEntity(idOf(p));
    check(r, bodies(engine) == base, "11 cleanup -> no leaked bodies");
}

// ---------------------------------------------------------------- 12 save
inline void validateSave(Engine& engine, Results& r)
{
    auto& scene = *engine.getActiveScene();

    flecs::entity w = scene.createEntity("V_SavedWeapon");
    w.set(CS::WeaponComponent{7.0f, 70.0f});
    w.get_mut<CS::WeaponRuntimeComponent>().timeSinceLastShot = 99.0f;
    w.get_mut<CS::WeaponRuntimeComponent>().wantsToFire = true;

    flecs::entity ch = scene.createEntity("V_SavedCharacter");
    ch.set(CS::CharacterControllerComponent{});
    ch.get_mut<CS::CharacterControllerRuntimeComponent>().verticalVelocity = 12.0f;

    const bool saved = engine.saveActiveSceneAs(SAVE_PATH);
    check(r, saved, "12 save scene");
    check(r, !scene.isDirty(), "12 successful save marks scene clean");

    auto resolved = engine.getProject()->resolveResourcePath(SAVE_PATH);
    const std::string text = resolved ? Utils::FileReader::readFile(resolved->string()) : "";

    bool runtimeLeaked = false;
    for (std::string_view key : {"bodyId", "soundInstanceId", "timeSinceLastShot", "wantsToFire",
                                 "verticalVelocity", "pendingMove", "pendingJump", "isPlaying", "playRequested"})
    {
        if (text.find(key) != std::string::npos)
        {
            runtimeLeaked = true;
        }
    }

    check(r, !text.empty(), "12 saved file readable");
    check(r, !runtimeLeaked, "12 saved file contains no runtime state");
    check(r, text.find("\"character_controller\"") != std::string::npos &&
             text.find("\"weapon\"") != std::string::npos, "12 saved file contains authoring components");
}

// ---------------------------------------------------------------- 13 + 15 load / switch
inline void validateLoadAndSwitch(Engine& engine, Results& r)
{
    const size_t modelsBefore = engine.getAssetManager().getLoadedModelCount();

    const bool loaded = engine.loadScene(SAVE_PATH);
    check(r, loaded, "13 load saved scene");
    check(r, engine.getAssetManager().getLoadedModelCount() == modelsBefore, "C1 scene switch reuses loaded models");
    if (!loaded)
    {
        return;
    }

    auto& scene = *engine.getActiveScene();
    auto& world = scene.getWorld();

    const int physicsEntities = world.count<CS::PhysicsComponent>();
    check(r, physicsEntities == world.count<CS::PhysicsRuntimeComponent>(), "13 every PhysicsComponent has runtime after load");
    check(r, bodies(engine) == static_cast<uint32_t>(physicsEntities), "15 old scene's bodies destroyed on switch");

    const int controllers = world.count<CS::CharacterControllerComponent>();
    check(r, characters() == controllers, "15 old scene's CharacterVirtuals destroyed on switch");

    bool weaponOk = false;
    bool characterOk = false;

    world.each([&](flecs::entity e, const CS::NameComponent& name)
    {
        if (name.name == "V_SavedWeapon")
        {
            const auto* cfg = e.try_get<CS::WeaponComponent>();
            const auto* rt = e.try_get<CS::WeaponRuntimeComponent>();
            weaponOk = cfg && cfg->fireRate == 7.0f && cfg->maxRange == 70.0f &&
                       rt && rt->timeSinceLastShot == 0.0f && !rt->wantsToFire;
        }
        else if (name.name == "V_SavedCharacter")
        {
            const auto* rt = e.try_get<CS::CharacterControllerRuntimeComponent>();
            characterOk = rt && rt->character && rt->verticalVelocity == 0.0f;
        }
    });

    check(r, weaponOk, "13 weapon authoring restored with fresh runtime");
    check(r, characterOk, "13 character controller restored with fresh runtime");
    check(r, !scene.isDirty(), "13 loaded scene is clean");
}

// ---------------------------------------------------------------- 13 transactional failure
inline void validateFailedLoad(Engine& engine, Results& r)
{
    auto resolved = engine.getProject()->resolveResourcePath(BROKEN_PATH);
    if (!resolved)
    {
        check(r, false, "13 resolve broken scene path");
        return;
    }

    // First entity builds a body + character, second fails in SceneLoader
    {
        std::ofstream out(*resolved, std::ios::trunc);
        out << R"({
    "format_version": 2,
    "name": "validation_broken",
    "entities": [
        { "id": "0123456789abcdef0123456789abcdef", "name": "Good",
                    "model": "res://assets/models/Floor.glb",
          "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1],
          "physics": { "type": "static", "half_extent": [1,1,1] },
          "character_controller": {} },
        { "id": "fedcba9876543210fedcba9876543210", "name": "Bad",
          "position": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1],
          "physics": { "type": "wobbly", "half_extent": [1,1,1] } }
    ],
    "directional_light": { "direction": [0,-1,0], "color": [1,1,1], "intensity": 1 },
    "point_lights": []
})";
    }

    const CS::Scene* before = engine.getActiveScene();
    const uint32_t b = bodies(engine);
    const int c = characters();
    auto& assets = engine.getAssetManager();
    const Assets::ModelHandle floor = assets.loadModel("res://assets/models/Floor.glb");
    const uint32_t floorRefs = assets.getModelRefCount(floor);

    const bool loaded = engine.loadScene(BROKEN_PATH);
    check(r, !loaded, "13 broken scene is rejected");
    check(r, engine.getActiveScene() == before, "13 failed load keeps current scene active");
    check(r, bodies(engine) == b, "13 failed load leaks no Jolt bodies");
    check(r, characters() == c, "13 failed load leaks no CharacterVirtuals");
    check(r, assets.getModelRefCount(floor) == floorRefs, "F2 failed load releases its model references");
}

// ---------------------------------------------------------------- 15 sim state on switch
inline void validateSimStateOnSwitch(Engine& engine, Results& r)
{
    engine.setSimulationState(SimulationState::Paused);

    const bool loaded = engine.loadScene(SAVE_PATH);
    bool disabled = false;
    if (loaded)
    {
        auto system = engine.getActiveScene()->getWorld().lookup("PhysicsSyncSystem");
        disabled = system && system.has(flecs::Disabled);
    }

    check(r, loaded && disabled, "15 new scene respects paused simulation state");
    engine.setSimulationState(SimulationState::Running);
}

// ---------------------------------------------------------------- 14 unload
inline void validateUnload(Engine& engine, Results& r)
{
    engine.unloadScene();
    check(r, !engine.hasActiveScene(), "14 unload -> no active scene");
    check(r, bodies(engine) == 0, "14 unload -> all Jolt bodies destroyed");
    check(r, characters() == 0, "14 unload -> all CharacterVirtuals destroyed");
    check(r, sounds() == 0, "14 unload -> all sounds stopped");
    check(r, engine.getAssetManager().getTotalModelRefCount() == 0, "14 unload -> all model references released");
    check(r, engine.getAssetManager().getLoadedModelCount() == 0, "14 unload -> all unused models freed");

    engine.createEmptyScene("Validation");
}

// ---------------------------------------------------------------- 16 shutdown
inline void validateShutdown(Engine& engine, Results& r)
{
    auto& scene = *engine.getActiveScene();

    flecs::entity p = scene.createEntity("V_ShutdownPhysics");
    p.set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.5f), 1.0f});

    flecs::entity c = scene.createEntity("V_ShutdownCharacter");
    c.set(CS::CharacterControllerComponent{});

    engine.shutdown(); // a crash here is a failure

    check(r, engine.getState() == EngineState::Uninitialized, "16 shutdown completes");
    check(r, characters() == 0, "16 shutdown destroys all CharacterVirtuals");
    check(r, sounds() == 0, "16 shutdown leaves no active sounds");
    check(r, engine.getAssetManager().getLoadedModelCount() == 0, "16 shutdown frees all model assets");
}

// ---------------------------------------------------------------- asset manager
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

// ---------------------------------------------------------------- B mesh lifecycle
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

// ---------------------------------------------------------------- B bad model inside a scene file
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

// ---------------------------------------------------------------- C refcounting
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

// ---------------------------------------------------------------- D unused collection
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

    // Active scene currently uses both helmet and floor
    const Assets::ModelHandle floorBefore = assets.loadModel("res://assets/models/Floor.glb");
    const Assets::ModelHandle helmetBefore = assets.loadModel("res://assets/models/DamagedHelmet.glb");

    const bool loaded = engine.loadScene(floorOnlyPath);
    check(r, loaded, "D0 floor-only scene loads");

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
}

// ---------------------------------------------------------------- G asset types
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

// ---------------------------------------------------------------- entry
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

    auto& scene = *engine.getActiveScene();
    validatePhysics(engine, scene, r);
    validateCharacter(scene, r);
    validateWeapon(scene, r);
    validateAudio(engine, scene, r);
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

    validateSimStateOnSwitch(engine, r);
    validateUnload(engine, r);
    validateShutdown(engine, r);

    Log::Print("==== RESULT: " + std::to_string(r.passed) + " passed, " + std::to_string(r.failed) + " failed ====",
               "Validation", r.failed ? LogType::LOG_ERROR : LogType::LOG_SUCCESS);

    return r.failed;
}
} // namespace Cthulhu::Validation