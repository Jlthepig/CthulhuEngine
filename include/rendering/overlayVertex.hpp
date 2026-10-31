#pragma once

#include <glm.hpp>

namespace Cthulhu::Rendering
{
struct OverlayVertex
{
    glm::vec3 position{};
    glm::vec4 color{1.0f};
};
} // namespace Cthulhu::Rendering
