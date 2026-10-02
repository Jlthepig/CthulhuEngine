#pragma once

#include "testCommon.hpp"
#include "session.hpp"

namespace Cthulhu::Validation
{

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

      events = session.takeEvents();
    session.undo();
    events = session.takeEvents();
    check(r, !scene->isDirty() && scene->getName() == "Scribe_E", "N8 undo back to the save point is clean");
    check(r, !events.empty() && events.back().type == ChangeType::SceneRenamed, "N9 undo emits change events");

    session.renameScene("Scribe_G");
    engine.loadScene(SAVE_PATH);
    check(r, !session.undo() && !session.canUndo(), "N10 scene switch clears history");
}
} // namespace Cthulhu::Validation