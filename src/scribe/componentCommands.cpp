#include <algorithm>
#include <cmath>

#include "components.hpp"
#include "componentCommands.hpp"
#include "assetManager.hpp"
#include "log_utils.hpp"
#include "scene.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scribe::Commands
{
namespace
{
struct Target
{
    flecs::entity entity;
    const Scene::ComponentDescriptor *descriptor = nullptr;
};

Result findTarget(Context &context, Scene::EntityId id, const std::string &component, bool allowCore, Target &out)
{
    const auto *descriptor = context.scene.getComponentRegistry()->find(component);
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
    AddComponentCommand(Scene::EntityId entityId, std::string componentName)
        : id(entityId),
          component(std::move(componentName)),
          label("Add " + component)
    {}

    std::string_view name() const override
    {
        return label;
    }

    Result apply(Context &context) override
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

    void revert(Context &context) override
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
    RemoveComponentCommand(Scene::EntityId entityId, std::string componentName)
        : id(entityId),
          component(std::move(componentName)),
          label("Remove " + component)
    {}

    std::string_view name() const override
    {
        return label;
    }

    Result apply(Context &context) override
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

    void revert(Context &context) override
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
        : id(entityId),
          component(std::move(componentName)),
          field(std::move(fieldName)),
          requested(std::move(value)),
          label("Set " + component + "." + field)
    {}

    std::string_view name() const override
    {
        return label;
    }

    Result apply(Context &context) override
    {
        Target target;
        if (Result found = findTarget(context, id, component, true, target); !found.ok())
        {
            return found;
        }

        const auto &registry = *context.scene.getComponentRegistry();

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

    void revert(Context &context) override
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

    bool mergeWith(const Command &next) override
    {
        const auto *edit = dynamic_cast<const SetFieldCommand *>(&next);
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

class SetTransformsCommand final : public Command
{
  public:
    explicit SetTransformsCommand(std::vector<TransformValue> values) : requested(std::move(values))
    {
        std::sort(requested.begin(), requested.end(), [](const TransformValue &a, const TransformValue &b) {
            return a.id.high < b.id.high || (a.id.high == b.id.high && a.id.low < b.id.low);
        });
    }

    SetTransformsCommand(std::vector<TransformValue> initial, std::vector<TransformValue> values) : SetTransformsCommand(std::move(values))
    {
        before = std::move(initial);
    }

    std::string_view name() const override
    {
        return "Transform Entities";
    }

    Result apply(Context &context) override
    {
        before.clear();
        before.reserve(requested.size());

        for (const auto &value : requested)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!std::isfinite(value.position[axis]) || !std::isfinite(value.rotation[axis]) ||
                    !std::isfinite(value.scale[axis]))
                {
                    return Result::failed("NON-FINITE TRANSFORM");
                }
            }

            if (!before.empty() && before.back().id == value.id)
            {
                return Result::failed("DUPLICATE TRANSFORM ENTITY");
            }

            const auto entity = context.scene.findEntity(value.id);
            const auto *transform = entity ? entity->try_get<Scene::TransformComponent>() : nullptr;
            if (!transform)
            {
                return Result::failed("ENTITY HAS NO TRANSFORM");
            }

            before.push_back({value.id, transform->position, transform->rotation, transform->scale});
        }

        return writeTransforms(context, requested);
    }

    void revert(Context &context) override
    {
        const Result result = writeTransforms(context, before);
        if (!result.ok())
        {
            Log::Print("UNDO TRANSFORMS FAILED: " + result.error, "Scribe", LogType::LOG_ERROR);
        }
    }

  private:
    std::vector<TransformValue> requested;
    std::vector<TransformValue> before;
};
class ReplaceReferencesCommand final : public Command
{
  public:
    ReplaceReferencesCommand(std::string fromPath, std::string toPath)
        : from(std::move(fromPath)),
          to(std::move(toPath))
    {}

    std::string_view name() const override
    {
        return "Replace References";
    }

    Result apply(Context &context) override
    {
        if (!to.empty())
        {
            auto key = Assets::normaliseResourcePath(to);
            if (!key)
            {
                return Result::failed("BAD PATH: " + to);
            }

            if (Assets::normaliseResourcePath(from) == key)
            {
                return Result::noChange();
            }
            to = *key;
        }

        const auto &registry = *context.scene.getComponentRegistry();
        const auto references = context.scene.findAssetReferences(from);
        if (references.empty())
        {
            return Result::noChange();
        }

        // check every field first so a wrong type changes nothing
        const Assets::AssetType newType = to.empty() ? Assets::AssetType::Unknown : Assets::getAssetType(to);
        for (const auto &ref : references)
        {
            const auto *descriptor = registry.find(ref.component);
            const auto *field = descriptor ? Scene::findField(*descriptor, ref.field) : nullptr;
            if (!field)
            {
                return Result::failed("UNKNOWN FIELD " + ref.component + "." + ref.field);
            }

            if (!to.empty() && field->assetType != newType)
            {
                return Result::failed("WRONG ASSET TYPE FOR " + ref.component + "." + ref.field);
            }
        }

        changed.clear();
        for (const auto &ref : references)
        {
            auto entity = context.scene.findEntity(ref.entity);
            auto old = entity ? registry.getField(*entity, ref.component, ref.field) : std::nullopt;
            if (!old || !registry.setField(*entity, ref.component, ref.field, Scene::FieldValue{to}))
            {
                continue;
            }

            changed.push_back({ref, std::get<std::string>(*old)});
            context.emit(ChangeType::ComponentChanged, ref.entity, ref.component);
        }

        return changed.empty() ? Result::noChange() : Result::applied();
    }

    void revert(Context &context) override
    {
        const auto &registry = *context.scene.getComponentRegistry();

        for (const auto &change : changed)
        {
            auto entity = context.scene.findEntity(change.reference.entity);
            Scene::ComponentSnapshot snapshot{change.reference.component,
                                              {{change.reference.field, Scene::FieldValue{change.oldValue}}}};

            if (!entity || !registry.apply(*entity, {snapshot}))
            {
                Log::Print("UNDO REPLACE FAILED: " + change.reference.component, "Scribe", LogType::LOG_ERROR);
                continue;
            }

            context.emit(ChangeType::ComponentChanged, change.reference.entity, change.reference.component);
        }
    }

  private:
    struct Change
    {
        Scene::AssetReference reference;
        std::string oldValue;
    };

    std::string from;
    std::string to;
    std::vector<Change> changed;
};

} // namespace

Result writeTransforms(Context &context, std::span<const TransformValue> values)
{
    for (const auto &value : values)
    {
        const auto entity = context.scene.findEntity(value.id);
        if (!entity || !entity->has<Scene::TransformComponent>())
        {
            return Result::failed("ENTITY HAS NO TRANSFORM");
        }
    }

    bool changed = false;
    for (const auto &value : values)
    {
        const auto entity = context.scene.findEntity(value.id);
        if (!entity || !entity->has<Scene::TransformComponent>())
        {
            return Result::failed("ENTITY LOST ITS TRANSFORM DURING GROUP EDIT");
        }

        auto transform = entity->get<Scene::TransformComponent>();
        if (transform.position == value.position && transform.rotation == value.rotation &&
            transform.scale == value.scale)
        {
            continue;
        }

        transform.position = value.position;
        transform.rotation = value.rotation;
        transform.scale = value.scale;
        transform.matrixDirty = true;
        entity->set(transform);
        context.emit(ChangeType::ComponentChanged, value.id, "Transform");
        changed = true;
    }

    return changed ? Result::applied() : Result::noChange();
}

std::unique_ptr<Command> addComponent(Scene::EntityId id, std::string component)
{
    return std::make_unique<AddComponentCommand>(id, std::move(component));
}

std::unique_ptr<Command> removeComponent(Scene::EntityId id, std::string component)
{
    return std::make_unique<RemoveComponentCommand>(id, std::move(component));
}

std::unique_ptr<Command> setField(Scene::EntityId id, std::string component, std::string field, Scene::FieldValue value)
{
    return std::make_unique<SetFieldCommand>(id, std::move(component), std::move(field), std::move(value));
}

std::unique_ptr<Command> setTransforms(std::vector<TransformValue> values)
{
    return std::make_unique<SetTransformsCommand>(std::move(values));
}

std::unique_ptr<Command> completedTransformEdit(std::vector<TransformValue> before, std::vector<TransformValue> after)
{
    return std::make_unique<SetTransformsCommand>(std::move(before), std::move(after));
}

std::unique_ptr<Command> replaceReferences(std::string from, std::string to)
{
    return std::make_unique<ReplaceReferencesCommand>(std::move(from), std::move(to));
}
} // namespace Cthulhu::Scribe::Commands
