#include <string>

#include "engine.hpp"
#include "scene.hpp"
#include "session.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scribe
{
namespace
{
class RenameSceneCommand final : public Command
{
  public:
    explicit RenameSceneCommand(std::string requestedName) : newName(std::move(requestedName))
    {
    }

    std::string_view name() const override
    {
        return "Rename Scene";
    }

    Result apply(Context &context) override
    {
        if (newName.empty())
        {
            return Result::failed("SCENE NAME CANNOT BE EMPTY");
        }

        oldName = context.scene.getName();
        if (oldName == newName)
        {
            return Result::noChange();
        }

        context.scene.setName(newName);
        context.emit(ChangeType::SceneRenamed);
        return Result::applied();
    }

    void revert(Context &context) override
    {
        context.scene.setName(oldName);
        context.emit(ChangeType::SceneRenamed);
    }

    bool mergeWith(const Command &next) override
    {
        const auto *rename = dynamic_cast<const RenameSceneCommand *>(&next);
        if (!rename)
        {
            return false;
        }

        newName = rename->newName;
        return true;
    }

  private:
    std::string newName;
    std::string oldName;
};
} // namespace

Session::Session(Engine &engine) : engine(engine), boundGeneration(engine.getSceneGeneration())
{
}

Scene::Scene *Session::syncScene()
{
    const uint64_t generation = engine.getSceneGeneration();
    if (generation != boundGeneration)
    {
        boundGeneration = generation;
        history.clear();
        events.push_back({ChangeType::SceneReplaced});
    }
    return engine.getActiveScene();
}

void Session::refreshDirty(Scene::Scene &scene)
{
    if (history.isAtSavePoint())
    {
        scene.markClean();
    }
    else
    {
        scene.markDirty();
    }
}

Result Session::execute(std::unique_ptr<Command> command)
{
    Scene::Scene *scene = syncScene();
    if (!scene)
    {
        return Result::failed("NO ACTIVE SCENE");
    }

    if (!command)
    {
        return Result::failed("NULL COMMAND");
    }

    Context context{engine, *scene, events};
    Result result = command->apply(context);

    if (result.status == Result::Status::Applied)
    {
        history.record(std::move(command));
        refreshDirty(*scene);
    }
    else if (result.status == Result::Status::Failed)
    {
        Log::Print(std::string(command->name()) + " FAILED: " + result.error, "Scribe", LogType::LOG_ERROR);
    }

    return result;
}

bool Session::undo()
{
    Scene::Scene *scene = syncScene();
    if (!scene)
    {
        return false;
    }

    Context context{engine, *scene, events};
    if (!history.undo(context))
    {
        return false;
    }

    refreshDirty(*scene);
    return true;
}

bool Session::redo()
{
    Scene::Scene *scene = syncScene();
    if (!scene)
    {
        return false;
    }

    Context context{engine, *scene, events};
    if (!history.redo(context))
    {
        return false;
    }

    refreshDirty(*scene);
    return true;
}

Result Session::save()
{
    if (!syncScene())
    {
        return Result::failed("NO ACTIVE SCENE");
    }

    if (!engine.saveActiveScene())
    {
        return Result::failed("SAVE FAILED");
    }

    history.markSaved();
    return Result::applied();
}

Result Session::saveAs(std::string_view resourcePath)
{
    if (!syncScene())
    {
        return Result::failed("NO ACTIVE SCENE");
    }

    if (!engine.saveActiveSceneAs(resourcePath))
    {
        return Result::failed("SAVE FAILED: " + std::string(resourcePath));
    }

    history.markSaved();
    return Result::applied();
}

std::vector<ChangeEvent> Session::takeEvents()
{
    syncScene();

    std::vector<ChangeEvent> drained;
    drained.swap(events);
    return drained;
}

Result Session::renameScene(std::string_view newName)
{
    return execute(std::make_unique<RenameSceneCommand>(std::string(newName)));
}
} // namespace Cthulhu::Scribe