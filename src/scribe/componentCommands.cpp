#include "componentCommands.hpp"
#include "scene.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scribe::Commands
{
namespace
{
struct Target
{
    flecs::entity entity;
    const Scene::ComponentDescriptor* descriptor = nullptr;
};

Result findTarget(Context& context, Scene::EntityId id, const std::string& component, bool allowCore, Target& out)
{
    const auto* descriptor = context.scene.getComponentRegistry()->find(component);
    if (!descriptor)
    {
        return Result::failed("UNKNOWN COMPONENT: " + component);
    }

    if (descriptor->core && !allowCore)
    {
        return Result::failed("CORE COMPONENT CANNOT BE ADDED OR REMOVED: " + component);
    }

    auto entity = context.scene.findEntity(id);
    if (!entity)
    {
        return Result::failed("ENTITY DOES NOT EXIST");
    }

    out = {*entity, descriptor};
    return Result::applied();
}

class AddComponentCommand final : public Command
{
  public:
    AddComponentCommand(Scene::EntityId entityId, std::string componentName) : id(entityId), component(std::move(componentName)), label("Add " + component)
    {
    }

    std::string_view name() const override
    {
        return label;
    }

    Result apply(Context& context) override
    {
        Target target;
        if (Result found = findTarget(context, id, component, false, target); !found.ok())
        {
            return found;
        }

        if (target.descriptor->has(target.entity))
        {
            return Result::noChange();
        }

        target.descriptor->add(target.entity);
        context.emit(ChangeType::ComponentAdded, id, component);
        return Result::applied();
    }

    void revert(Context& context) override
    {
        Target target;
        if (!findTarget(context, id, component, false, target).ok())
        {
            Log::Print("UNDO ADD FAILED: " + component, "Scribe", LogType::LOG_ERROR);
            return;
        }

        target.descriptor->remove(target.entity);
        context.emit(ChangeType::ComponentRemoved, id, component);
    }

  private:
    Scene::EntityId id;
    std::string component;
    std::string label;
};

class RemoveComponentCommand final : public Command
{
  public:
    RemoveComponentCommand(Scene::EntityId entityId, std::string componentName) : id(entityId), component(std::move(componentName)), label("Remove " + component)
    {
    }

    std::string_view name() const override
    {
        return label;
    }

    Result apply(Context& context) override
    {
        Target target;
        if (Result found = findTarget(context, id, component, false, target); !found.ok())
        {
            return found;
        }

        auto captured = context.scene.getComponentRegistry()->captureComponent(target.entity, component);
        if (!captured)
        {
            return Result::noChange();
        }

        saved = std::move(*captured);
        target.descriptor->remove(target.entity);
        context.emit(ChangeType::ComponentRemoved, id, component);
        return Result::applied();
    }

    void revert(Context& context) override
    {
        auto entity = context.scene.findEntity(id);
        if (!entity || !context.scene.getComponentRegistry()->apply(*entity, {saved}))
        {
            Log::Print("UNDO REMOVE FAILED: " + component, "Scribe", LogType::LOG_ERROR);
            return;
        }

        context.emit(ChangeType::ComponentAdded, id, component);
    }

  private:
    Scene::EntityId id;
    std::string component;
    std::string label;
    Scene::ComponentSnapshot saved;
};

class SetFieldCommand final : public Command
{
  public:
    SetFieldCommand(Scene::EntityId entityId, std::string componentName, std::string fieldName, Scene::FieldValue value)
        : id(entityId), component(std::move(componentName)), field(std::move(fieldName)), requested(std::move(value)),
          label("Set " + component + "." + field)
    {
    }

    std::string_view name() const override
    {
        return label;
    }

    Result apply(Context& context) override
    {
        Target target;
        if (Result found = findTarget(context, id, component, true, target); !found.ok())
        {
            return found;
        }

        const auto& registry = *context.scene.getComponentRegistry();

        auto current = registry.getField(target.entity, component, field);
        if (!current)
        {
            return Result::failed("ENTITY HAS NO FIELD " + component + "." + field);
        }

        if (*current == requested)
        {
            return Result::noChange();
        }

        if (!registry.setField(target.entity, component, field, requested))
        {
            return Result::failed("INVALID VALUE FOR " + component + "." + field);
        }

        if (registry.getField(target.entity, component, field) == current)
        {
            return Result::noChange();
        }

        oldValue = std::move(*current);
        context.emit(ChangeType::ComponentChanged, id, component);
        return Result::applied();
    }

    void revert(Context& context) override
    {
        auto entity = context.scene.findEntity(id);
        Scene::ComponentSnapshot snapshot{component, {{field, oldValue}}};

        // apply() writes the old value exactly <<EVEN if it happened to be outside the field limits>>
        if (!entity || !context.scene.getComponentRegistry()->apply(*entity, {snapshot}))
        {
            Log::Print("UNDO FAILED: " + label, "Scribe", LogType::LOG_ERROR);
            return;
        }

        context.emit(ChangeType::ComponentChanged, id, component);
    }

    bool mergeWith(const Command& next) override
    {
        const auto* edit = dynamic_cast<const SetFieldCommand*>(&next);
        if (!edit || edit->id != id || edit->component != component || edit->field != field)
        {
            return false;
        }

        requested = edit->requested;
        return true;
    }

  private:
    Scene::EntityId id;
    std::string component;
    std::string field;
    Scene::FieldValue requested;
    std::string label;
    Scene::FieldValue oldValue;
};
} // namespace

std::unique_ptr<Command> addComponent(Scene::EntityId id, std::string component)
{
    return std::make_unique<AddComponentCommand>(id, std::move(component));
}

std::unique_ptr<Command> removeComponent(Scene::EntityId id, std::string component)
{
    return std::make_unique<RemoveComponentCommand>(id, std::move(component));
}

std::unique_ptr<Command> setField(Scene::EntityId id, std::string component, std::string field,Scene::FieldValue value)
{
    return std::make_unique<SetFieldCommand>(id, std::move(component), std::move(field), std::move(value));
}
} // namespace Cthulhu::Scribe::Commands