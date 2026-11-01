#pragma once

#include <memory>
#include <string>
#include <vector>
#include <span>

#include "command.hpp"
#include "componentRegistry.hpp"
#include "transformValue.hpp"

namespace Cthulhu::Scribe::Commands
{
std::unique_ptr<Command> addComponent(Scene::EntityId id, std::string component);
std::unique_ptr<Command> removeComponent(Scene::EntityId id, std::string component);
std::unique_ptr<Command> setField(Scene::EntityId id, std::string component, std::string field, Scene::FieldValue value);
std::unique_ptr<Command> setTransforms(std::vector<TransformValue> values);

Result writeTransforms(Context &context, std::span<const TransformValue> values);
std::unique_ptr<Command> completedTransformEdit(std::vector<TransformValue> before, std::vector<TransformValue> after);

std::unique_ptr<Command> replaceReferences(std::string from, std::string to);
} // namespace Cthulhu::Scribe::Commands
