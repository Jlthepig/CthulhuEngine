#pragma once

#include <stdexcept>
#include <chrono>

#include "session.hpp"
#include "testCommon.hpp"

namespace Cthulhu::Validation
{
inline int startupRegistrations = 0;

inline void validateClampedMerge(Engine &engine, Results &r)
{
    if (!engine.createEmptyScene("ClampedMerge"))
    {
        check(r, false, "Audit: clamped merge setup");
        return;
    }
    auto &scene = *engine.getActiveScene();
    auto entity = scene.createEntity("ClampedField");
    entity.set(CS::WeaponComponent{0.0f, 50.0f});
    scene.markClean();
    Scribe::Session session(engine);
    const auto id = idOf(entity);
    const bool changed = session.setField(id, "Weapon", "fireRate", 3.0f).ok();
    const bool returned = session.setField(id, "Weapon", "fireRate", -4.0f).ok();
    check(r, changed && returned && entity.get<CS::WeaponComponent>().fireRate == 0.0f &&
                 !scene.isDirty() && !session.canUndo(),
          "Audit: merged clamped edit returning to the initial value leaves no dirty history");
    session.endMerge();
    const bool edited = session.setField(id, "Weapon", "fireRate", 5.0f).ok();
    const bool clamped = session.setField(id, "Weapon", "fireRate", -2.0f).ok();
    check(r, edited && clamped && !session.canUndo() && !scene.isDirty(),
          "Audit: repeated clamped gestures preserve the save point");
    session.endMerge();
    entity.get_mut<CS::WeaponComponent>().fireRate = 5.0f;
    entity.modified<CS::WeaponComponent>();
    scene.markClean();
    const bool applied = session.setField(id, "Weapon", "fireRate", -1000000.0f).ok();
    const float actual = entity.get<CS::WeaponComponent>().fireRate;
    const bool undone = session.undo();
    check(r, applied && actual == 0.0f && undone && !scene.isDirty() && session.redo() &&
                 entity.get<CS::WeaponComponent>().fireRate == actual && scene.isDirty(),
          "Audit: redo restores the applied clamped value");
}

inline void validateSnapshotTraversal(Engine &engine, Results &r)
{
    CS::Scene scene(engine.getComponentRegistry());
    for (int i = 0; i < 256; ++i)
        scene.createEntity("Root" + std::to_string(i));
    auto roots = scene.getRootEntities();
    const auto alphabetical = scene.captureScene();
    bool defaultOrder = alphabetical.entities.size() == roots.size();
    for (std::size_t index = 0; index < alphabetical.entities.size(); ++index)
        defaultOrder = defaultOrder && alphabetical.entities[index].id == roots[index] &&
                       alphabetical.entities[index].siblingOrder == index &&
                       !alphabetical.entities[index].hasCustomSiblingOrder;
    check(r, defaultOrder, "Audit: default snapshot order stays alphabetical without custom metadata");
    scene.setSiblingOrder(roots.back(), 0);
    roots = scene.getRootEntities();
    for (int i = 0; i < 16; ++i)
        scene.setParent(idOf(scene.createEntity("Child" + std::to_string(i))), roots.front());
    const auto children = scene.getChildren(roots.front());
    scene.setSiblingOrder(children.back(), 0);
    const auto started = std::chrono::steady_clock::now();
    const auto snapshot = scene.captureScene();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count();
    bool valid = snapshot.entities.size() == 272;
    for (const auto &entity : snapshot.entities)
        valid = valid && entity.siblingOrder == scene.getSiblingOrder(entity.id) &&
                entity.hasCustomSiblingOrder == scene.hasCustomSiblingOrder(entity.parentId);
    const auto subtree = scene.captureSubtree(roots.back());
    check(r, valid && subtree && subtree->front().siblingOrder == roots.size() - 1,
          "Audit: scene and subtree capture preserve sibling positions and ordering modes");
    CS::Scene restored(engine.getComponentRegistry());
    check(r, restored.restoreScene(snapshot) && restored.getRootEntities() == roots &&
                 restored.getChildren(roots.front()) == scene.getChildren(roots.front()),
          "Audit: wide hierarchy snapshot restores the same root and child order");
    Log::Print("Audit capture (272 entities): " + std::to_string(elapsed) + " us", "Validation", LogType::LOG_INFO);
}

inline void validateDuplicationGrowth(Engine &engine, Results &r)
{
    CS::Scene scene(engine.getComponentRegistry());
    const auto source = idOf(scene.createEntity("GrowthSource"));
    const auto child = idOf(scene.createEntity("GrowthChild"));
    bool valid = scene.setParent(child, source);
    for (int i = 0; i < 512 && valid; ++i)
    {
        const auto duplicate = scene.duplicateEntity(source);
        const auto children = duplicate ? scene.getChildren(*duplicate) : std::vector<CS::EntityId>{};
        valid = duplicate && children.size() == 1 && children.front() != child;
    }
    check(r, valid && scene.getChildren(source) == std::vector<CS::EntityId>{child},
          "Audit: duplication survives component storage growth");
}

inline void validateOrderedDeleteUndo(Engine &engine, Results &r)
{
    if (!engine.createEmptyScene("OrderedUndo"))
    {
        check(r, false, "Audit: ordered undo setup");
        return;
    }
    Scribe::Session session(engine);
    auto &scene = *engine.getActiveScene();
    const auto parent = session.createEntity("Parent");
    const auto first = session.createEntity("First", parent.id);
    const auto second = session.createEntity("Second", parent.id);
    const bool ordered = scene.setSiblingOrder(second.id, 0);
    const auto before = scene.getChildren(parent.id);
    const bool deleted = session.deleteEntity(second.id).ok();
    check(r, parent.ok() && first.ok() && second.ok() && ordered && deleted && session.undo() &&
                 scene.getChildren(parent.id) == before,
          "Audit: undo restores one child into an existing custom sibling group");

    const bool rootsOrdered = scene.setSiblingOrder(parent.id, 0);
    const auto other = session.createEntity("OtherRoot");
    const auto roots = scene.getRootEntities();
    const bool rootDeleted = session.deleteEntity(parent.id).ok();
    check(r, rootsOrdered && other.ok() && rootDeleted && session.undo() &&
                 scene.getRootEntities() == roots && scene.getChildren(parent.id) == before,
          "Audit: undo restores a subtree into an existing custom root group");

    const auto third = session.createEntity("Third", parent.id);
    const auto fourth = session.createEntity("Fourth", parent.id);
    const auto fullOrder = scene.getChildren(parent.id);
    auto restored = scene.captureSubtree(first.id);
    const auto another = scene.captureSubtree(third.id);
    if (!restored || !another || !fourth.ok())
    {
        check(r, false, "Audit: partial sibling restore setup");
        return;
    }
    restored->insert(restored->end(), another->begin(), another->end());
    scene.destroyEntity(first.id);
    scene.destroyEntity(third.id);
    const auto remaining = scene.getChildren(parent.id);
    scene.markClean();
    auto invalid = *restored;
    invalid.back().siblingOrder = invalid.front().siblingOrder;
    check(r, !scene.restoreSubtree(invalid) && scene.getChildren(parent.id) == remaining && !scene.isDirty(),
          "Audit: duplicate sibling positions roll back without disturbing survivors");
    invalid = *restored;
    invalid.back().siblingOrder = fullOrder.size();
    check(r, !scene.restoreSubtree(invalid) && scene.getChildren(parent.id) == remaining && !scene.isDirty(),
          "Audit: out-of-range sibling positions roll back without disturbing survivors");
    check(r, scene.restoreSubtree(*restored) && scene.getChildren(parent.id) == fullOrder,
          "Audit: partial multi-entity restore preserves unaffected sibling order");
}

struct ValidationTransitionField
{
    int value = 0;
};

inline Engine *transitionTestEngine = nullptr;
inline bool transitionAccepted = false;
inline bool queueTransition = false;
inline bool transitionQueued = false;

inline void validateCommandTransitions(Engine &engine, Results &r)
{
    auto descriptor = CS::makeComponentDescriptor<ValidationTransitionField>("V_TransitionGuard");
    descriptor.fields.push_back(CS::makeField(
        "value", CS::FieldType::Int,
        [](flecs::entity entity) -> CS::FieldValue {
            return entity.get<ValidationTransitionField>().value;
        },
        [](flecs::entity entity, const CS::FieldValue &value) {
            entity.set(ValidationTransitionField{std::get<int>(value)});
            transitionAccepted = transitionTestEngine->createEmptyScene("UnsafeTransition");
            if (transitionAccepted)
            {
                // Stop before the unfixed command can dereference its destroyed world.
                throw std::runtime_error("unguarded scene transition");
            }
            if (queueTransition)
            {
                transitionQueued = transitionTestEngine->requestTransition(
                    EngineTransition::CreateEmptyScene, "QueuedTransition");
            }
        }));
    if (!engine.getComponentRegistry().registerComponent(std::move(descriptor)) ||
        !engine.createEmptyScene("CommandTransitions"))
    {
        check(r, false, "Audit: command transition setup");
        return;
    }

    const auto mode = engine.getWorldMode();
    engine.setWorldMode(WorldMode::Edit);
    Scribe::Session session(engine);
    const auto created = session.createEntity("TransitionTarget");
    session.addComponent(created.id, "V_TransitionGuard");
    const auto generation = engine.getSceneGeneration();
    transitionTestEngine = &engine;
    transitionAccepted = false;
    queueTransition = false;
    transitionQueued = false;
    bool applied = false, undone = false, redone = false, queuedEdit = false;
    try
    {
        applied = session.setField(created.id, "V_TransitionGuard", "value", 1).ok();
        undone = session.undo();
        redone = session.redo();
        queueTransition = true;
        queuedEdit = session.setField(created.id, "V_TransitionGuard", "value", 2).ok();
    }
    catch (const std::runtime_error &)
    {
        // An accepted transition is the regression failure, not a process crash.
    }
    transitionTestEngine = nullptr;
    queueTransition = false;
    check(r, applied && undone && redone && queuedEdit && !transitionAccepted &&
                 engine.getSceneGeneration() == generation,
          "Audit: command apply/undo/redo block immediate scene transitions");
    engine.update();
    check(r, transitionQueued && engine.getActiveScene() &&
                 engine.getActiveScene()->getName() == "QueuedTransition",
          "Audit: a command can queue a transition for the next frame boundary");
    engine.setWorldMode(mode);
    check(r, engine.loadScene(SAVE_PATH), "Audit: restore fixture after command transitions");
}
} // namespace Cthulhu::Validation
