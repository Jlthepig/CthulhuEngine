#include "entityCommands.hpp"
#include "scene.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scribe::Commands
{
namespace
{
std::string currentName(const Scene::Scene &scene, Scene::EntityId id)
{
    auto entity = scene.findEntity(id);
    const auto *name = entity ? entity->try_get<Scene::NameComponent>() : nullptr;
    return name ? name->name : "Entity";
}

class CreateEntityCommand final : public Command
{
    public:
        CreateEntityCommand(Scene::EntityId id, std::string name, std::optional<Scene::EntityId> parent): id(id), requestedName(std::move(name)), parent(parent)
        {}   
        
        std::string_view name() const override
        {
            return "Create Entity";
        }

        Result apply(Context &context) override
        {
            auto &scene = context.scene;

            if (parent && !scene.isEntityAlive(*parent))
            {
                return Result::failed("PARENT ENTITY DOES NOT EXIST");
            }

            if (!scene.createEntityWithId(id, requestedName.empty() ? "Entity" : requestedName))
            {
                return Result::failed("FAILED TO CREATE ENTITY");
            }

            if (parent && !scene.setParent(id, *parent))
            {
                scene.destroyEntity(id);
                return Result::failed("FAILED TO PARENT NEW ENTITY");
            }

            context.emit(ChangeType::EntityCreated, id);
            return Result::applied();
        }

        void revert(Context &context) override
        {
            context.scene.destroyEntity(id);
            context.emit(ChangeType::EntityDestroyed, id);
        }

    private:
        Scene::EntityId id;
        std::string requestedName;
        std::optional<Scene::EntityId> parent;
}; 

class DeleteEntityCommand final : public Command
{
    public:
        explicit DeleteEntityCommand(Scene::EntityId id): id(id)
        {}

        std::string_view name() const override
        {
            return "Delete Entity";
        }

        Result apply(Context &context) override
        {
            auto snapshot = context.scene.captureSubtree(id);
            if (!snapshot)
            {
                return Result::failed("ENTITY DOES NOT EXIST");
            }

            subtree = std::move(*snapshot);
            context.scene.destroyEntity(id);

            for (auto it = subtree.rbegin(); it != subtree.rend(); ++it)
            {
                context.emit(ChangeType::EntityDestroyed, it->id);
            }
            return Result::applied();
        }

        void revert(Context &context) override
        {
            if (!context.scene.restoreSubtree(subtree))
            {
                Log::Print("UNDO DELETE FAILED TO RESTORE SUBTREE","Scribe",LogType::LOG_ERROR);
                return;
            }

            for (const auto &snapshot : subtree)
            {
                context.emit(ChangeType::EntityCreated, snapshot.id);
            }
        }

    private:
        Scene::EntityId id;
        std::vector<Scene::EntitySnapshot> subtree;
};

class DuplicateEntityCommand final : public Command
{
  public:
    explicit DuplicateEntityCommand(Scene::EntityId source) : source(source)
    {
    }

    std::string_view name() const override
    {
        return "Duplicate Entity";
    }

    Result apply(Context &context) override
    {
        auto &scene = context.scene;

        if (subtree.empty())
        {
            auto duplicateId = scene.duplicateEntity(source);
            if (!duplicateId)
            {
                return Result::failed("FAILED TO DUPLICATE ENTITY");
            }

            auto snapshot = scene.captureSubtree(*duplicateId);
            if (!snapshot)
            {
                scene.destroyEntity(*duplicateId);
                return Result::failed("FAILED TO CAPTURE DUPLICATE");
            }
            subtree = std::move(*snapshot);
        }
        else if (!scene.restoreSubtree(subtree))
        {
            return Result::failed("FAILED TO RESTORE DUPLICATE");
        }

        for (const auto &snapshot : subtree) // root first
        {
            context.emit(ChangeType::EntityCreated, snapshot.id);
        }
        return Result::applied();
    }

    void revert(Context &context) override
    {
        context.scene.destroyEntity(subtree.front().id);

        for (auto it = subtree.rbegin(); it != subtree.rend(); ++it)
        {
            context.emit(ChangeType::EntityDestroyed, it->id);
        }
    }

  private:
    Scene::EntityId source;
    std::vector<Scene::EntitySnapshot> subtree;
};

class RenameEntityCommand final : public Command
{
  public:
    RenameEntityCommand(Scene::EntityId id, std::string name) : id(id), newName(std::move(name))
    {
    }

    std::string_view name() const override
    {
        return "Rename Entity";
    }

    Result apply(Context &context) override
    {
        if (newName.empty())
        {
            return Result::failed("ENTITY NAME CANNOT BE EMPTY");
        }

        if (!context.scene.isEntityAlive(id))
        {
            return Result::failed("ENTITY DOES NOT EXIST");
        }

        oldName = currentName(context.scene, id);
        if (oldName == newName)
        {
            return Result::noChange();
        }

        context.scene.renameEntity(id, newName);
        context.emit(ChangeType::EntityRenamed, id);
        return Result::applied();
    }

    void revert(Context &context) override
    {
        context.scene.renameEntity(id, oldName);
        context.emit(ChangeType::EntityRenamed, id);
    }

    bool mergeWith(const Command &next) override
    {
        const auto *rename = dynamic_cast<const RenameEntityCommand *>(&next);
        if (!rename || rename->id != id)
        {
            return false;
        }

        newName = rename->newName;
        return true;
    }

  private:
    Scene::EntityId id;
    std::string newName;
    std::string oldName;
};

class ReparentEntityCommand final : public Command
{
  public:
    ReparentEntityCommand(Scene::EntityId child, std::optional<Scene::EntityId> parent)
        : child(child), newParent(parent)
    {
    }

    std::string_view name() const override
    {
        return "Reparent Entity";
    }

    Result apply(Context &context) override
    {
        auto &scene = context.scene;

        if (!scene.isEntityAlive(child))
        {
            return Result::failed("ENTITY DOES NOT EXIST");
        }

        oldParent = scene.getParent(child);
        if (oldParent == newParent)
        {
            return Result::noChange();
        }

        const bool changed = newParent ? scene.setParent(child, *newParent) : scene.clearParent(child);
        if (!changed)
        {
            return Result::failed("INVALID REPARENT (MISSING PARENT OR HIERARCHY CYCLE)");
        }

        context.emit(ChangeType::EntityReparented, child);
        return Result::applied();
    }

    void revert(Context &context) override
    {
        if (oldParent)
        {
            context.scene.setParent(child, *oldParent);
        }
        else
        {
            context.scene.clearParent(child);
        }
        context.emit(ChangeType::EntityReparented, child);
    }

  private:
    Scene::EntityId child;
    std::optional<Scene::EntityId> newParent;
    std::optional<Scene::EntityId> oldParent;
};
}  // namespace

std::unique_ptr<Command> createEntity(Scene::EntityId id, std::string name, std::optional<Scene::EntityId> parent)
{
    return std::make_unique<CreateEntityCommand>(id, std::move(name), parent);
}

std::unique_ptr<Command> deleteEntity(Scene::EntityId id)
{
    return std::make_unique<DeleteEntityCommand>(id);
}

std::unique_ptr<Command> duplicateEntity(Scene::EntityId source)
{
    return std::make_unique<DuplicateEntityCommand>(source);
}

std::unique_ptr<Command> renameEntity(Scene::EntityId id, std::string name)
{
    return std::make_unique<RenameEntityCommand>(id, std::move(name));
}

std::unique_ptr<Command> reparentEntity(Scene::EntityId child, std::optional<Scene::EntityId> parent)
{
    return std::make_unique<ReparentEntityCommand>(child, parent);
}

} // namespace Cthulhu::Scribe::Commands