#pragma once

#include <array>

#include "../integration/testsEditorDay.hpp"

namespace Cthulhu::Validation
{
struct ClipboardLink
{
    CS::EntityId target;
};

inline void validateEntityClipboard(Engine &engine, Results &r)
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

    auto &registry = engine.getComponentRegistry();

    if (!registry.find("V_ClipboardLink"))
    {
        auto descriptor =
            CS::makeComponentDescriptor<ClipboardLink>("V_ClipboardLink");

        descriptor.fields.push_back(CS::makeField(
            "target", CS::FieldType::EntityRef,
            [](flecs::entity e) -> CS::FieldValue {
                return e.get<ClipboardLink>().target;
            },
            [](flecs::entity e, const CS::FieldValue &value) {
                e.set(ClipboardLink{std::get<CS::EntityId>(value)});
            }));

        if (!registry.registerComponent(std::move(descriptor)))
        {
            check(r, false, "Clipboard: register reference descriptor");
            return;
        }
    }

    if (!engine.createEmptyScene("Clipboard"))
    {
        check(r, false, "Clipboard: create scene");
        return;
    }

    auto &scene = *engine.getActiveScene();

    const auto group = idOf(scene.createEntity("Group"));
    const auto box = idOf(scene.createEntity("Box"));
    const auto bolt = idOf(scene.createEntity("Bolt"));
    const auto other = idOf(scene.createEntity("Crate 1"));
    const auto outside = idOf(scene.createEntity("Outside"));
    const auto tail = idOf(scene.createEntity("Tail"));

    if (!scene.setParent(box, group) ||
        !scene.setParent(tail, group) ||
        !scene.setParent(bolt, box) ||
        !scene.setSiblingOrder(tail, 0))
    {
        check(r, false, "Clipboard: create hierarchy");
        return;
    }

    scene.findEntity(box)->set(ClipboardLink{other});
    scene.findEntity(other)->set(ClipboardLink{bolt});
    scene.findEntity(bolt)->set(ClipboardLink{outside});

    registry.setField(
        *scene.findEntity(box), "Transform", "position", glm::vec3(1, 2, 3));
    registry.setField(
        *scene.findEntity(box), "Transform", "shear",
        glm::vec3(0.2f, 0.1f, 0.3f));

    scene.markClean();

    Scribe::Session session(engine);
    const std::array selection{bolt, box, other, box};

    const auto original = fingerprint(scene);
    const auto siblingOrder = scene.getChildren(group);
    const auto count = scene.getEntityCount();

    check(r, session.copyEntities(selection).ok() &&
                 !scene.isDirty() && !session.canUndo(),
          "Clipboard: copy changes neither scene nor history");

    auto duplicate = session.duplicateEntities(selection);

    check(r, duplicate.result.ok() && duplicate.ids.size() == 2 &&
                 scene.getEntityCount() == count + 3,
          "Clipboard: selected descendants and repeated IDs copy only once");

    if (duplicate.ids.size() != 2)
        return;

    const auto children = scene.getChildren(duplicate.ids[0]);

    check(r, scene.findEntityByName("Box 1") == duplicate.ids[0] &&
                 scene.findEntityByName("Crate 2") == duplicate.ids[1] &&
                 children.size() == 1 &&
                 scene.getParent(duplicate.ids[0]) == group,
          "Clipboard: numbered names and original parent");

    if (children.size() != 1)
        return;

    check(r,
          scene.findEntity(duplicate.ids[0])->get<ClipboardLink>().target ==
              duplicate.ids[1] &&
          scene.findEntity(duplicate.ids[1])->get<ClipboardLink>().target ==
              children[0] &&
          scene.findEntity(children[0])->get<ClipboardLink>().target == outside,
          "Clipboard: remap internal references and retain live external references");

    const auto duplicated = fingerprint(scene);

    check(r, session.undo() && !scene.isDirty(),
          "Clipboard: one undo removes the whole duplicate");
    expectSame(r, fingerprint(scene), original,
               "Clipboard: duplicate undo restores all fields");
    check(r, scene.getChildren(group) == siblingOrder,
          "Clipboard: duplicate undo preserves sibling order");

    check(r, session.redo(), "Clipboard: duplicate redo");
    expectSame(r, fingerprint(scene), duplicated,
               "Clipboard: redo keeps the same IDs and values");

    if (!session.undo())
        return;

    const std::array invalid{box, CS::EntityId{}};

    check(r, !session.deleteEntities(invalid).ok() &&
                 !session.copyEntities(invalid).ok() && !scene.isDirty(),
          "Clipboard: invalid selections do not partially mutate or replace the clipboard");
    expectSame(r, fingerprint(scene), original,
               "Clipboard: invalid operations leave the scene unchanged");

    check(r, session.setField(
                 box, "Transform", "position", glm::vec3(9)).ok(),
          "Clipboard: edit original after copying");

    auto pasted = session.pasteEntities();

    if (pasted.ids.size() != 2)
    {
        check(r, false, "Clipboard: paste saved snapshot");
        return;
    }

    check(r,
          scene.findEntity(pasted.ids[0])->get<CS::TransformComponent>().position ==
              glm::vec3(1, 2, 3),
          "Clipboard: paste uses copy-time values");

    auto again = session.pasteEntities();

    check(r, again.ids.size() == 2 &&
                 scene.findEntityByName("Box 2") == again.ids[0],
          "Clipboard: repeated paste advances numbering");

    if (!session.undo() || !session.undo() || !session.undo())
        return;

    check(r, session.deleteEntities(selection).ok() &&
                 scene.getEntityCount() == count - 3 &&
                 session.undo() &&
                 scene.getChildren(group) == siblingOrder,
          "Clipboard: grouped delete restores custom sibling positions in one undo");
    expectSame(r, fingerprint(scene), original,
               "Clipboard: grouped delete undo restores all fields");

    check(r, session.copyEntities(selection, true).ok() &&
                 scene.getEntityCount() == count - 3,
          "Clipboard: cut removes the selection in one operation");

    auto moved = session.pasteEntities();

    check(r, moved.ids.size() == 2 &&
                 scene.findEntityByName("Box") == moved.ids[0],
          "Clipboard: cut-paste retains an available name");

    if (!session.undo() || !session.undo())
        return;

    expectSame(r, fingerprint(scene), original,
               "Clipboard: undo paste then cut restores originals");

    const auto switched = session.newSceneDocument();

    if (!switched.result.ok())
    {
        check(r, false, "Clipboard: switch document: " + switched.result.error);
        return;
    }

    auto across = session.pasteEntities();
    auto &destination = *engine.getActiveScene();

    if (across.ids.size() != 2)
    {
        check(r, false, "Clipboard: paste across scene tabs");
        return;
    }

    const auto copiedChildren = destination.getChildren(across.ids[0]);

    check(r, !destination.getParent(across.ids[0]) &&
                 copiedChildren.size() == 1 &&
                 destination.findEntity(across.ids[0])->get<ClipboardLink>().target ==
                     across.ids[1] &&
                 !destination.findEntity(copiedChildren[0])->get<ClipboardLink>()
                      .target.isValid(),
          "Clipboard: cross-document paste makes roots and clears external entity references");

    if (engine.play())
    {
        check(r, !session.copyEntities(across.ids).ok() &&
                     !session.pasteEntities().result.ok() &&
                     !session.deleteEntities(across.ids).ok(),
              "Clipboard: scene operations are rejected during Play");

        check(r, engine.stop(), "Clipboard: return to Edit");
    }
    else
    {
        check(r, false, "Clipboard: start Play");
    }
}
}
