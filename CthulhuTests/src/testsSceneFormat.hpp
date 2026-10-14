#pragma once

#include <algorithm>
#include <fstream>
#include <optional>
#include <vector>

#include "testCommon.hpp"
#include "testsDescriptors.hpp"
#include "testsEditorDay.hpp"

namespace Cthulhu::Validation
{
inline constexpr std::string_view FORMAT_PATH = "res://.cthulhu/validation_scene_format.scene";

inline bool writeFormat4SceneText(Engine& engine, const std::string& entities)
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

    const auto orderParent = scene->createEntity("V_OrderParent");
    const auto orderAlpha = scene->createEntity("V_Order_A");
    const auto orderZulu = scene->createEntity("V_Order_Z");
    const CS::EntityId orderParentId = idOf(orderParent);
    const CS::EntityId orderAlphaId = idOf(orderAlpha);
    const CS::EntityId orderZuluId = idOf(orderZulu);
    scene->setParent(orderAlphaId, orderParentId);
    scene->setParent(orderZuluId, orderParentId);
    const bool customOrderSet = scene->setSiblingOrder(orderZuluId, 0);

    const ScenePrint before = fingerprint(*scene);
    const bool saved = engine.saveActiveSceneAs(FORMAT_PATH);
    auto resolved = engine.getProject()->resolveResourcePath(FORMAT_PATH);
    const std::string text = resolved ? Utils::FileReader::readFile(resolved->string()) : "";
    check(r, customOrderSet && saved && text.find("\"format_version\": 5") != std::string::npos &&
                 text.find("\"sibling_order\": 0") != std::string::npos,
          "F1 format 5 saves custom sibling order");
    check(r, text.find("\"type\": \"Dynamic\"") != std::string::npos, "F2 enums are saved by name");

    check(r, engine.loadScene(FORMAT_PATH), "F3 a format 5 scene loads");
    expectSame(r, fingerprint(*engine.getActiveScene()), before, "F3 Camera and game components survive save + load");
    const auto restoredOrder = engine.getActiveScene()->getChildren(orderParentId);
    check(r, restoredOrder.size() == 2 && restoredOrder[0] == orderZuluId && restoredOrder[1] == orderAlphaId,
          "F3 custom sibling order survives save + load");

    const bool oldOrderWritten = writeFormat4SceneText(engine, R"(
        { "id": "0123456789abcdef0123456789abcdef", "name": "V_Old_Z", "components": {} },
        { "id": "1123456789abcdef0123456789abcdef", "name": "V_Old_A", "components": {} }
    )");
    const bool oldOrderLoaded = oldOrderWritten && engine.loadScene(FORMAT_PATH);
    const auto *oldScene = engine.getActiveScene();
    const auto oldRoots = oldScene ? oldScene->getRootEntities() : std::vector<CS::EntityId>{};
    const auto oldAlphaId = oldScene ? oldScene->findEntityByName("V_Old_A") : std::nullopt;
    const auto oldZuluId = oldScene ? oldScene->findEntityByName("V_Old_Z") : std::nullopt;
    const auto oldAlphaPosition = oldAlphaId ? std::find(oldRoots.begin(), oldRoots.end(), *oldAlphaId) : oldRoots.end();
    const auto oldZuluPosition = oldZuluId ? std::find(oldRoots.begin(), oldRoots.end(), *oldZuluId) : oldRoots.end();
    check(r, oldOrderLoaded && oldAlphaPosition != oldRoots.end() && oldZuluPosition != oldRoots.end() &&
                 oldAlphaPosition < oldZuluPosition,
          "F3 format 4 scenes without sibling order remain alphabetical");

    const bool lenientWritten = writeFormat4SceneText(engine, R"(
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

    check(r, writeFormat4SceneText(engine, R"(
        { "id": "0123456789abcdef0123456789abcdef", "name": "V_Bad", "components": {
            "Weapon": { "fireRate": "fast" } } })") && !engine.loadScene(FORMAT_PATH),
          "F5 a wrong value type is rejected");

    check(r, writeFormat4SceneText(engine, R"(
        { "id": "0123456789abcdef0123456789abcdef", "name": "V_Bad", "components": {
            "Physics": { "type": "Wobbly" } } })") && !engine.loadScene(FORMAT_PATH),
          "F5 an unknown enum name is rejected");

    engine.loadScene(SAVE_PATH);
}
} // namespace Cthulhu::Validation