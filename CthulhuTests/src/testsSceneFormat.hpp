#pragma once

#include <fstream>

#include "testCommon.hpp"
#include "testsDescriptors.hpp"
#include "testsEditorDay.hpp"

namespace Cthulhu::Validation
{
inline constexpr std::string_view FORMAT_PATH = "res://.cthulhu/validation_v4.scene";

inline bool writeSceneText(Engine& engine, const std::string& entities)
{
    auto resolved = engine.getProject()->resolveResourcePath(FORMAT_PATH);
    if (!resolved)
    {
        return false;
    }

    std::ofstream out(*resolved, std::ios::trunc);
    out << R"({ "format_version": 4, "name": "validation_v4", "entities": [ )" << entities
        << R"( ], "directional_light": { "direction": [0,-1,0], "color": [1,1,1], "intensity": 1 }, "point_lights": [] })";
    return out.good();
}

// needs V_Health from validateDescriptorSnapshots
inline void validateSceneFormat(Engine& engine, Results& r)
{
    if (!engine.createEmptyScene("V_Format"))
    {
        check(r, false, "F0 format test setup");
        return;
    }

    auto* scene = engine.getActiveScene();
    flecs::entity camera = scene->createEntity("V_Camera");
    camera.set(CS::CameraComponent{glm::vec3(0.0f, -0.5f, -0.75f)});
    camera.set(ValidationHealth{12.5f, true});
    flecs::entity crate = scene->createEntity("V_Crate");
    crate.set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.25f), 3.0f});
    scene->setParent(idOf(crate), idOf(camera));

    const ScenePrint before = fingerprint(*scene);
    const bool saved = engine.saveActiveSceneAs(FORMAT_PATH);
    auto resolved = engine.getProject()->resolveResourcePath(FORMAT_PATH);
    const std::string text = resolved ? Utils::FileReader::readFile(resolved->string()) : "";
    check(r, saved && text.find("\"format_version\": 4") != std::string::npos, "F1 scenes are saved as format 4");
    check(r, text.find("\"type\": \"Dynamic\"") != std::string::npos, "F2 enums are saved by name");

    check(r, engine.loadScene(FORMAT_PATH), "F3 a format 4 scene loads");
    expectSame(r, fingerprint(*engine.getActiveScene()), before, "F3 Camera and game components survive save + load");

    const bool lenientWritten = writeSceneText(engine, R"(
        { "id": "0123456789abcdef0123456789abcdef", "name": "V_Lenient", "components": {
            "Transform": { "position": [1, 2, 3] },
            "Weapon": { "fireRate": 4, "noSuchField": 1 },
            "NoSuchComponent": { "x": 1 } } })");
    const bool lenientLoaded = lenientWritten && engine.loadScene(FORMAT_PATH);
    auto lenientId = lenientLoaded ? engine.getActiveScene()->findEntityByName("V_Lenient") : std::nullopt;
    auto lenient = lenientId ? engine.getActiveScene()->findEntity(*lenientId) : std::nullopt;
    const auto* weapon = lenient ? lenient->try_get<CS::WeaponComponent>() : nullptr;
    const auto* transform = lenient ? lenient->try_get<CS::TransformComponent>() : nullptr;
    check(r, weapon && weapon->fireRate == 4.0f && weapon->maxRange == CS::WeaponComponent{}.maxRange && transform &&
                 transform->position == glm::vec3(1.0f, 2.0f, 3.0f) && transform->scale == glm::vec3(1.0f),
          "F4 missing fields keep defaults; unknown fields and components are skipped");

    check(r, writeSceneText(engine, R"(
        { "id": "0123456789abcdef0123456789abcdef", "name": "V_Bad", "components": {
            "Weapon": { "fireRate": "fast" } } })") && !engine.loadScene(FORMAT_PATH),
          "F5 a wrong value type is rejected");

    check(r, writeSceneText(engine, R"(
        { "id": "0123456789abcdef0123456789abcdef", "name": "V_Bad", "components": {
            "Physics": { "type": "Wobbly" } } })") && !engine.loadScene(FORMAT_PATH),
          "F5 an unknown enum name is rejected");

    engine.loadScene(SAVE_PATH);
}
} // namespace Cthulhu::Validation