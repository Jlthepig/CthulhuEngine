#pragma once

#include <glm.hpp>

#include "entityId.hpp"

namespace Cthulhu::Scribe
{

struct TransformValue
{
    Scene::EntityId id{};
    glm::vec3 position{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.0f};
    glm::vec3 shear{0.0f};

    bool operator==(const TransformValue &) const = default;
};
} // namespace Cthulhu::Scribe