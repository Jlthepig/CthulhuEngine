#pragma once

#include <limits>

#include "testCommon.hpp"
#include "session.hpp"

namespace Cthulhu::Validation
{

inline void validateScribeDocumentSwitch(Engine &engine, Results &r)
{
    struct RestoreWorldMode
    {
        Engine &engine;
        WorldMode previous;

        ~RestoreWorldMode()
        {
            engine.setWorldMode(previous);
        }
    } restoreMode{engine, engine.getWorldMode()};

    engine.setWorldMode(WorldMode::Edit);
    if (!engine.createEmptyScene("Tab A"))
    {
        check(r, false, "Scene tabs can create the test document");
        return;
    }

    Scribe::Session session(engine);
    check(r, !engine.getActiveScene()->needsSave() && !engine.unsavedEditSceneName(),
          "An untouched untitled scene does not prompt to save");
    const auto entity = session.createEntity("Tab A root");
    check(r, entity.ok() && engine.getActiveScene()->needsSave() && engine.unsavedEditSceneName().has_value(),
          "Editing an untitled scene prompts to save");
    const bool renamed = entity.ok() && session.renameEntity(entity.id, "Renamed root").ok();
    const bool canRestoreRedo = renamed && session.undo() && session.canRedo();
    check(r, canRestoreRedo, "Scene tabs have a dirty document with undo and redo");

    const auto generationA = engine.getSceneGeneration();
    const auto rejectedOpen = session.openSceneDocument(TEST_AUDIO);
    check(r, !rejectedOpen.result.ok() && !rejectedOpen.previous &&
                 engine.getSceneGeneration() == generationA &&
                 engine.getActiveScene()->isEntityAlive(entity.id) && session.canUndo() && session.canRedo(),
          "Scene tabs preserve the active document after an invalid open");

    auto opened = session.openSceneDocument(SAVE_PATH);
    if (!opened.result.ok() || !opened.previous)
    {
        check(r, false, "Scene tabs open the saved fixture and capture the outgoing document");
        return;
    }

    const auto changes = session.takeEvents();
    check(r, !session.canUndo() && !session.canRedo() && !engine.getActiveScene()->isDirty() &&
                 changes.size() == 1 && changes.front().type == Scribe::ChangeType::SceneReplaced,
          "Opening a scene starts its own history and emits one scene replacement");

    const auto originalNameB = engine.getActiveScene()->getName();
    const bool editedB = session.renameScene("Tab B").ok();
    const auto generationB = engine.getSceneGeneration();

    auto &tabA = opened.previous;
    tabA->snapshot.entities.push_back({});
    const auto rejectedRestore = session.activateSceneDocument(tabA);
    check(r, !rejectedRestore.result.ok() && tabA && !rejectedRestore.previous &&
                 engine.getSceneGeneration() == generationB &&
                 engine.getActiveScene()->getName() == "Tab B" && session.canUndo(),
          "Scene tabs preserve both documents after a failed restore");

    if (!tabA)
        return;

    tabA->snapshot.entities.pop_back();
    auto switched = session.activateSceneDocument(tabA);
    if (!switched.result.ok() || !switched.previous)
    {
        check(r, false, "Scene tabs restore the inactive document");
        return;
    }

    check(r, !tabA && engine.getActiveScene()->getName() == "Tab A" &&
                 engine.getActiveScene()->isDirty() && engine.getActiveScene()->isEntityAlive(entity.id) &&
                 session.canUndo() && session.redo(),
          "Scene tabs restore the entity ID, dirty state, undo and redo");

    auto switchedBack = session.activateSceneDocument(switched.previous);
    check(r, editedB && switchedBack.result.ok() && session.undo() &&
                 engine.getActiveScene()->getName() == originalNameB && !engine.getActiveScene()->isDirty(),
          "Scene tabs restore the second document's history and save point");

    if (!switchedBack.result.ok() || !switchedBack.previous)
        return;

    auto &inactive = switchedBack.previous;
    const auto generation = engine.getSceneGeneration();
    const auto closedInactive = session.closeInactiveSceneDocument(inactive);
    check(r, closedInactive.ok() && !inactive && engine.getSceneGeneration() == generation &&
                 engine.getActiveScene()->getName() == originalNameB && session.canRedo(),
          "Closing an inactive tab preserves the active scene and history");

    if (!closedInactive.ok())
        return;

    const auto closedActive = session.closeActiveSceneDocument();
    const auto closeEvents = session.takeEvents();
    check(r, closedActive.ok() && !engine.hasActiveScene() && !session.canRedo() &&
                 closeEvents.size() == 1 && closeEvents.front().type == Scribe::ChangeType::SceneReplaced,
          "Closing the final tab unloads the scene and clears history");
}

inline void validateScribeEntities(Engine& engine, Results& r)
{
    using Scribe::ChangeType;

    Scribe::Session session(engine);
    check(r, engine.loadScene(SAVE_PATH), "O0 setup: load saved scene");
    (void) session.takeEvents();

    auto* scene = engine.getActiveScene();
    const uint32_t baseBodies = bodies(engine);

    auto nameOf = [&](CS::EntityId id) {
        auto e = scene->findEntity(id);
        const auto* n = e ? e->try_get<CS::NameComponent>() : nullptr;
        return n ? n->name : std::string{};
    };

    const auto parent = session.createEntity("O_Parent");
    auto events = session.takeEvents();
    check(r, parent.ok() && scene->isEntityAlive(parent.id) && events.size() == 1 &&
             events[0].type == ChangeType::EntityCreated && events[0].entityId == parent.id,
          "O1 create entity emits EntityCreated");

    session.undo();
    const bool goneAfterUndo = !scene->isEntityAlive(parent.id);
    session.redo();
    check(r, goneAfterUndo && scene->isEntityAlive(parent.id), "O1 undo removes it, redo restores the same ID");

    const auto child = session.createEntity("O_Child", parent.id);
    check(r, child.ok() && scene->getParent(child.id) == parent.id, "O2 create entity under a parent");

    scene->findEntity(parent.id)->set(CS::PhysicsComponent{CS::PhysicsBodyType::Dynamic, glm::vec3(0.75f), 2.0f});
    scene->findEntity(child.id)->set(CS::MeshComponent{"res://assets/models/Floor.glb"});
    const uint32_t withBodies = bodies(engine);

    check(r, session.deleteEntity(parent.id).ok() && !scene->isEntityAlive(parent.id) &&
             !scene->isEntityAlive(child.id) && bodies(engine) == withBodies - 1,
          "O3 delete removes the whole subtree and its runtime");

    session.undo();
    const auto restoredParent = scene->findEntity(parent.id);
    const auto restoredChild = scene->findEntity(child.id);
    const auto* physics = restoredParent ? restoredParent->try_get<CS::PhysicsComponent>() : nullptr;
    check(r, restoredParent && restoredChild && scene->getParent(child.id) == parent.id && physics &&
             physics->halfExtent == glm::vec3(0.75f) && physics->mass == 2.0f &&
             restoredParent->has<CS::PhysicsRuntimeComponent>() && restoredChild->has<CS::MeshRuntimeComponent>() &&
             bodies(engine) == withBodies,
          "O3 undo delete restores same IDs, hierarchy, components and runtime");

    session.endMerge();
    session.renameEntity(child.id, "O_R1");
    session.renameEntity(child.id, "O_R12");
    session.endMerge();
    const std::string typed = nameOf(child.id);
    session.undo();
    check(r, typed == "O_R12" && nameOf(child.id) == "O_Child", "O4 rename merges while typing and undoes as one step");

    check(r, session.reparentEntity(child.id, std::nullopt).ok() && !scene->getParent(child.id), "O5 detach entity");
    session.undo();
    check(r, scene->getParent(child.id) == parent.id, "O5 undo detach restores the parent");
      {
            const auto orderParent = session.createEntity("O_IndexedParent");
            const auto orderChild = session.createEntity("O_IndexedChild", orderParent.id);
            const auto siblingA = session.createEntity("O_IndexedSibling_A", orderParent.id);
            const auto siblingB = session.createEntity("O_IndexedSibling_B", orderParent.id);
            const auto destination = session.createEntity("O_IndexedDestination");
            const auto destinationA = session.createEntity("O_IndexedDestination_A", destination.id);
            const auto destinationB = session.createEntity("O_IndexedDestination_B", destination.id);
            const bool fixturesReady =
                  orderParent.ok() && orderChild.ok() && siblingA.ok() && siblingB.ok() &&
                  destination.ok() && destinationA.ok() && destinationB.ok();

            const auto sourceBeforeReorder = scene->getChildren(orderParent.id);
            const bool sourceWasCustomBeforeReorder =
                  scene->hasCustomSiblingOrder(orderParent.id);
            const auto oldPosition = scene->getSiblingOrder(orderChild.id);
            const auto reorderResult = session.reparentEntity(orderChild.id, orderParent.id, 2);
            const bool reorderApplied =
                  reorderResult.status == Scribe::Result::Status::Applied;
            const auto reorderedChildren = scene->getChildren(orderParent.id);
            const auto reorderedPosition = scene->getSiblingOrder(orderChild.id);

            check(r, fixturesReady && sourceBeforeReorder.size() == 3 &&
                               !sourceWasCustomBeforeReorder && oldPosition && *oldPosition != 2 &&
                               reorderApplied && reorderedPosition && *reorderedPosition == 2 &&
                               reorderedChildren.size() == 3 && reorderedChildren[2] == orderChild.id &&
                               scene->hasCustomSiblingOrder(orderParent.id),
                    "O5 indexed same-parent reorder reaches the requested position");

            const bool reorderUndone = reorderApplied && session.undo();
            check(r, reorderUndone &&
                               scene->getChildren(orderParent.id) == sourceBeforeReorder &&
                               scene->hasCustomSiblingOrder(orderParent.id) == sourceWasCustomBeforeReorder,
                    "O5 undo restores sibling order and custom/default mode");

            const auto sourceBeforeCross = scene->getChildren(orderParent.id);
            const auto destinationBeforeCross = scene->getChildren(destination.id);
            const bool sourceWasCustomBeforeCross =
                  scene->hasCustomSiblingOrder(orderParent.id);
            const bool destinationWasCustomBeforeCross =
                  scene->hasCustomSiblingOrder(destination.id);

            const auto crossResult = session.reparentEntity(orderChild.id, destination.id, 1);
            const bool crossApplied =
                  crossResult.status == Scribe::Result::Status::Applied;
            const auto destinationAfterCross = scene->getChildren(destination.id);
            const auto crossPosition = scene->getSiblingOrder(orderChild.id);

            check(r, crossApplied && crossPosition && *crossPosition == 1 &&
                               scene->getParent(orderChild.id) == destination.id &&
                               destinationAfterCross.size() == 3 &&
                               destinationAfterCross[1] == orderChild.id &&
                               !destinationWasCustomBeforeCross &&
                               scene->hasCustomSiblingOrder(destination.id),
                    "O5 indexed cross-parent reparent reaches the requested position");

            const bool crossUndone = crossApplied && session.undo();
            check(r, crossUndone && scene->getParent(orderChild.id) == orderParent.id &&
                               scene->getChildren(orderParent.id) == sourceBeforeCross &&
                               scene->getChildren(destination.id) == destinationBeforeCross &&
                               scene->hasCustomSiblingOrder(orderParent.id) == sourceWasCustomBeforeCross &&
                               scene->hasCustomSiblingOrder(destination.id) == destinationWasCustomBeforeCross,
                    "O5 undo restores both sibling groups and their ordering modes");

            const auto rootsBeforeMove = scene->getRootEntities();
            const auto sourceBeforeRootMove = scene->getChildren(orderParent.id);
            const bool rootsWereCustom = scene->hasCustomSiblingOrder(std::nullopt);
            const bool sourceWasCustomBeforeRootMove =
                  scene->hasCustomSiblingOrder(orderParent.id);

            const auto rootResult = session.reparentEntity(orderChild.id, std::nullopt, 0);
            const bool rootMoveApplied =
                  rootResult.status == Scribe::Result::Status::Applied;
            const auto rootsAfterMove = scene->getRootEntities();
            const auto rootPosition = scene->getSiblingOrder(orderChild.id);
            const bool placedAtRootIndexZero =
                  rootMoveApplied && !scene->getParent(orderChild.id) &&
                  rootPosition && *rootPosition == 0 && !rootsAfterMove.empty() &&
                  rootsAfterMove[0] == orderChild.id &&
                  scene->hasCustomSiblingOrder(std::nullopt);

            const bool rootMoveUndone = rootMoveApplied && session.undo();
            check(r, placedAtRootIndexZero && rootMoveUndone &&
                               scene->getParent(orderChild.id) == orderParent.id &&
                               scene->getChildren(orderParent.id) == sourceBeforeRootMove &&
                               scene->getRootEntities() == rootsBeforeMove &&
                               scene->hasCustomSiblingOrder(std::nullopt) == rootsWereCustom &&
                               scene->hasCustomSiblingOrder(orderParent.id) == sourceWasCustomBeforeRootMove,
                    "O5 indexed move to root and undo restore both sibling groups");
      }
    check(r, !session.reparentEntity(parent.id, child.id).ok() && scene->getParent(child.id) == parent.id,
          "O5 reparent that would create a cycle is rejected");

    const auto dup = session.duplicateEntity(parent.id);
    const auto dupChildren = dup.ok() ? scene->getChildren(dup.id) : std::vector<CS::EntityId>{};
    check(r, dup.ok() && dup.id != parent.id && dupChildren.size() == 1 && dupChildren[0] != child.id,
          "O6 duplicate creates a new subtree with new IDs");

    session.undo();
    const bool dupGone = !scene->isEntityAlive(dup.id);
    session.redo();
    const auto redoChildren = scene->getChildren(dup.id);
    check(r, dupGone && scene->isEntityAlive(dup.id) && redoChildren.size() == 1 && !dupChildren.empty() &&
             redoChildren[0] == dupChildren[0],
          "O6 undo removes the duplicate, redo restores the same IDs");

    while (session.undo())
    {
    }
    check(r, !scene->isEntityAlive(parent.id) && !scene->isEntityAlive(dup.id) && bodies(engine) == baseBodies &&
             !scene->isDirty(),
          "O7 undoing all entity edits returns to the clean, loaded state");
}

inline void validateScribeCore(Engine& engine, Results& r)
{
    using Scribe::ChangeType;
    using Status = Scribe::Result::Status;

    Scribe::Session session(engine);

    check(r, engine.loadScene(SAVE_PATH), "N0 setup: load saved scene");
    auto events = session.takeEvents();
    check(r, events.size() == 1 && events[0].type == ChangeType::SceneReplaced, "N0 scene switch is announced");

    auto* scene = engine.getActiveScene();
    const std::string original = scene->getName();

    const auto renamed = session.renameScene("Scribe_A");
    events = session.takeEvents();
    check(r, renamed.status == Status::Applied && scene->getName() == "Scribe_A" && events.size() == 1 &&
             events[0].type == ChangeType::SceneRenamed && scene->isDirty(),
          "N1 command applies, emits an event and dirties the scene");

    const auto same = session.renameScene("Scribe_A");
    check(r, same.status == Status::NoChange && session.takeEvents().empty(), "N2 no-op edit is not recorded");

    const auto bad = session.renameScene("");
    check(r, !bad.ok() && scene->getName() == "Scribe_A", "N3 invalid edit fails and changes nothing");

    session.endMerge();
    session.renameScene("Scribe_B");
    session.renameScene("Scribe_BC"); // same interaction: merges
    session.endMerge();
    session.undo();
    check(r, scene->getName() == "Scribe_A", "N4 merged edits undo as one step");

    session.redo();
    check(r, scene->getName() == "Scribe_BC", "N5 redo reapplies");

    session.undo();
    session.undo();
    check(r, scene->getName() == original && !scene->isDirty() && !session.canUndo(),
          "N6 undoing everything returns to the clean, saved state");

    session.renameScene("Scribe_D");
    session.undo();
    session.renameScene("Scribe_E");
    check(r, !session.canRedo(), "N7 a new edit clears redo");

    check(r, session.saveAs("res://.cthulhu/validation_scribe.scene").ok() && !scene->isDirty(),
          "N8 save marks the scene clean");
    session.renameScene("Scribe_F");
    check(r, scene->isDirty(), "N8 edit after save dirties the scene");

    (void) session.takeEvents();
    session.undo();
    events = session.takeEvents();
    check(r, !scene->isDirty() && scene->getName() == "Scribe_E", "N8 undo back to the save point is clean");
    check(r, !events.empty() && events.back().type == ChangeType::SceneRenamed, "N9 undo emits change events");

    session.renameScene("Scribe_G");
    engine.loadScene(SAVE_PATH);
    check(r, !session.undo() && !session.canUndo(), "N10 scene switch clears history");
}

inline void validateScribeTransforms(Engine &engine, Results &r)
{
    using Scribe::TransformValue;
    using Status = Scribe::Result::Status;

    check(r, engine.loadScene(SAVE_PATH), "Transforms: load fixture");
    Scribe::Session session(engine);
    auto *scene = engine.getActiveScene();
    const auto first = session.createEntity("Transform_First");
    const auto second = session.createEntity("Transform_Second");
    check(r, scene && first.ok() && second.ok(), "Transforms: create two entities");
    if (!scene || !first.ok() || !second.ok())
    {
        return;
    }

    const auto read = [&](CS::EntityId id) -> std::optional<TransformValue> {
        const auto entity = scene->findEntity(id);
        const auto *transform = entity ? entity->try_get<CS::TransformComponent>() : nullptr;
        if (!transform)
        {
            return std::nullopt;
        }
        return TransformValue{id, transform->position, transform->rotation, transform->scale};
    };

    const TransformValue original[]{
        {first.id, {}, {}, glm::vec3{1.0f}},
        {second.id, {}, {}, glm::vec3{1.0f}}};
    const TransformValue requested[]{
        {first.id, {1.0f, 2.0f, 3.0f}, {0.1f, 0.2f, 0.3f}, glm::vec3{2.0f}},
        {second.id, {4.0f, 5.0f, 6.0f}, {0.4f, 0.5f, 0.6f}, glm::vec3{3.0f}}};
    const std::string previousUndo(session.undoName());

    const auto applied = session.setTransforms(requested);
    check(r, applied.status == Status::Applied &&
                 read(first.id) == requested[0] && read(second.id) == requested[1],
          "Transforms: apply the whole group");

    const bool undone = session.undo();
    check(r, undone && read(first.id) == original[0] && read(second.id) == original[1] &&
                 session.undoName() == previousUndo,
          "Transforms: one undo restores the whole group");

    const auto noChange = session.setTransforms(original);
    check(r, noChange.status == Status::NoChange && session.canRedo(),
          "Transforms: no-op preserves redo");

    const bool redone = session.redo();
    check(r, redone && read(first.id) == requested[0] && read(second.id) == requested[1],
          "Transforms: redo restores the whole group");

    TransformValue invalid[]{requested[0], requested[1]};
    invalid[0].position.x = 100.0f;
    invalid[1].id = {};
    const auto missing = session.setTransforms(invalid);
    check(r, !missing.ok() && read(first.id) == requested[0] && read(second.id) == requested[1],
          "Transforms: missing entity changes nothing");

    invalid[1] = invalid[0];
    const auto duplicate = session.setTransforms(invalid);
    check(r, !duplicate.ok() && read(first.id) == requested[0] && read(second.id) == requested[1],
          "Transforms: duplicate entity changes nothing");

    invalid[1] = requested[1];
    invalid[1].scale.z = std::numeric_limits<float>::infinity();
    const auto nonFinite = session.setTransforms(invalid);
    check(r, !nonFinite.ok() && read(first.id) == requested[0] && read(second.id) == requested[1],
          "Transforms: non-finite value changes nothing");

    const CS::EntityId selection[]{first.id, second.id};
    const std::string undoBeforePreview(session.undoName());
    const bool dirtyBeforePreview = scene->isDirty();

    const bool began = session.beginTransformEdit(selection).ok();
    check(r, began && session.transformEditActive(), "Preview: begin captures the selection");
    if (!began || session.transformStart().size() != 2)
    {
        return;
    }

    const auto captured = session.transformStart();
    std::vector<TransformValue> start(captured.begin(), captured.end());
    auto preview = start;
    for (auto &value : preview)
    {
        value.position.x += 2.0f;
    }

    const bool firstPreview = session.previewTransforms(preview).ok();
    for (std::size_t i = 0; i < preview.size(); ++i)
    {
        preview[i].position.x = start[i].position.x + 5.0f;
    }
    const bool secondPreview = session.previewTransforms(preview).ok();

    check(r, firstPreview && secondPreview &&
                 read(preview[0].id) == preview[0] && read(preview[1].id) == preview[1] &&
                 session.undoName() == undoBeforePreview,
          "Preview: repeated updates do not enter history");

    check(r, !session.canUndo() && !session.canRedo() &&
                 !session.undo() && !session.redo() && !session.renameEntity(first.id, "Blocked").ok(),
          "Preview: other history edits are blocked");

    auto invalidPreview = preview;
    invalidPreview[1].scale.z = std::numeric_limits<float>::infinity();
    check(r, !session.previewTransforms(invalidPreview).ok() &&
                 read(preview[0].id) == preview[0] && read(preview[1].id) == preview[1],
          "Preview: invalid values preserve the last valid preview");

    check(r, session.finishTransformEdit(false).ok() && !session.transformEditActive() &&
                 read(first.id) == requested[0] && read(second.id) == requested[1] &&
                 session.undoName() == undoBeforePreview && scene->isDirty() == dirtyBeforePreview,
          "Preview: cancel restores values, history and dirty state");

    const bool beganNoOp = session.beginTransformEdit(selection).ok();
    const bool movedNoOp = session.previewTransforms(preview).ok();
    const bool returnedNoOp = session.previewTransforms(start).ok();
    const auto noOpCommit = session.finishTransformEdit(true);
    check(r, beganNoOp && movedNoOp && returnedNoOp && noOpCommit.status == Status::NoChange &&
                 session.undoName() == undoBeforePreview && scene->isDirty() == dirtyBeforePreview,
          "Preview: returning to the start records nothing");

    const bool beganCommit = session.beginTransformEdit(selection).ok();
    const bool previewedCommit = session.previewTransforms(preview).ok();
    const auto committed = session.finishTransformEdit(true);
    check(r, beganCommit && previewedCommit && committed.status == Status::Applied &&
                 !session.transformEditActive(),
          "Preview: commit records the gesture");

    check(r, session.undo() && read(first.id) == requested[0] && read(second.id) == requested[1],
          "Preview: one undo restores both entities");

    const bool beganCancel = session.beginTransformEdit(selection).ok();
    const bool previewedCancel = session.previewTransforms(preview).ok();
    check(r, beganCancel && previewedCancel && session.finishTransformEdit(false).ok() && session.canRedo(),
          "Preview: cancellation preserves redo");

    check(r, session.redo() &&
                 read(preview[0].id) == preview[0] && read(preview[1].id) == preview[1],
          "Preview: redo reapplies the committed gesture");

    {
        Scribe::Session scoped(engine);
        const bool began = scoped.beginTransformEdit(selection).ok();
        const bool updated = scoped.previewTransforms(start).ok();
        check(r, began && updated, "Preview: scoped session can preview");
    }

    check(r, read(preview[0].id) == preview[0] && read(preview[1].id) == preview[1],
          "Preview: session destruction cancels an unfinished edit");

    const auto previousWorldMode = engine.getWorldMode();
    engine.setWorldMode(WorldMode::Edit);
    const bool beganPlay = session.beginTransformEdit(selection).ok();
    const bool previewedPlay = session.previewTransforms(start).ok();
    const bool cancelledPlay = session.finishTransformEdit(false).ok();
    const bool played = engine.play();
    const bool stopped = played && engine.stop();
    scene = engine.getActiveScene();
    engine.setWorldMode(previousWorldMode);
    check(r, beganPlay && previewedPlay && cancelledPlay && stopped &&
                 read(preview[0].id) == preview[0] && read(preview[1].id) == preview[1],
          "Preview: cancelling before Play preserves the edit scene");

    const bool beganReplacement = session.beginTransformEdit(selection).ok();
    const bool previewedReplacement = session.previewTransforms(start).ok();
    const bool replaced = engine.loadScene(SAVE_PATH);
    const auto stalePreview = session.previewTransforms(preview);
    scene = engine.getActiveScene();
    check(r, beganReplacement && previewedReplacement && replaced && !stalePreview.ok() &&
                 !session.transformEditActive() && session.transformStart().empty() && !session.canUndo(),
          "Preview: scene replacement discards the old gesture safely");

    while (session.undo())
    {
    }
}

} // namespace Cthulhu::Validation
