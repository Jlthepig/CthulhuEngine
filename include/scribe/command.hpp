#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "entityId.hpp"

namespace Cthulhu
{
class Engine;
}

namespace Cthulhu::Scene
{
class Scene;
}

namespace Cthulhu::Scribe
{
    struct Result
    {
        enum class Status : uint8_t
        {
            Applied,
            NoChange,
            Failed
        };

        Status status = Status::Applied;
        std::string error;

        [[nodiscard]] bool ok() const noexcept
        {
            return status != Status::Failed;
        }

        [[nodiscard]] static Result applied()
        {
            return {};
        }

        [[nodiscard]] static Result noChange()
        {
            return {Status::NoChange, {}};
        }

        [[nodiscard]] static Result failed(std::string message)
        {
            return {Status::Failed, std::move(message)};
        }
    };

    struct EntityResult
    {
        Result result;
        Scene::EntityId id{};

        [[nodiscard]] bool ok() const noexcept
        {
            return result.ok();
        }
    };

    enum class ChangeType : uint8_t
    {
        SceneReplaced, // active scene switched history cleared rebuild all ui
        SceneRenamed,
        EntityCreated,
        EntityDestroyed,
        EntityRenamed,
        EntityReparented,
        ComponentAdded,
        ComponentRemoved,
        ComponentChanged
    };

    struct ChangeEvent
    {
        ChangeType type = ChangeType::SceneReplaced;
        Scene::EntityId entityId{};
        std::string component{};
    };

    struct Context
    {
        Engine &engine;
        Scene::Scene &scene;
        std::vector<ChangeEvent> &events;

        void emit(ChangeType type, Scene::EntityId entityId = {}, std::string component = {})
        {
            events.push_back({type, entityId, std::move(component)});
        }
    };

    class Command
    {
    public:
        virtual ~Command() = default;

        [[nodiscard]] virtual std::string_view name() const = 0;
        
        virtual Result apply(Context &context) = 0;
        virtual void revert(Context &context) = 0;

        virtual bool mergeWith(const Command &)
        {
            return false;
        }
    };
} // namespace Cthulhu::Scribe