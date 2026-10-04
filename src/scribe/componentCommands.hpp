#pragma once

#include <memory>
#include <string>

#include "command.hpp"
#include "componentRegistry.hpp"

namespace Cthulhu::Scribe::Commands
{
    std::unique_ptr<Command> addComponent(Scene::EntityId id, std::string component);
    std::unique_ptr<Command> removeComponent(Scene::EntityId id, std::string component);
    std::unique_ptr<Command> setField(Scene::EntityId id, std::string component, std::string field,Scene::FieldValue value);
    std::unique_ptr<Command> replaceReferences(std::string from, std::string to);
}