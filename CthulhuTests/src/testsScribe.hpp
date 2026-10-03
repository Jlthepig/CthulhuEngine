#pragma once

#include "testCommon.hpp"
#include "session.hpp"

namespace Cthulhu::Validation
{

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
} // namespace Cthulhu::Validation