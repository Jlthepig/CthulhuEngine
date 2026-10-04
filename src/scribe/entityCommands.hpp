#pragma once 

#include <memory>
#include <optional>
#include <string>

#include "command.hpp"

namespace Cthulhu::Scribe::Commands
{
    std::unique_ptr<Command> createEntity(Scene::EntityId id, std::string name, std::optional<Scene::EntityId> parent);
    std::unique_ptr<Command> deleteEntity(Scene::EntityId id);
    std::unique_ptr<Command> renameEntity(Scene::EntityId id, std::string name);
    std::unique_ptr<Command> reparentEntity(Scene::EntityId child, std::optional<Scene::EntityId> parent);

    std::unique_ptr<Command> duplicateEntity(Scene::EntityId source);
}