#pragma once

#include <optional>
#include <string>

#include "model.hpp"
namespace Cthulhu::Rendering
{
class ModelLoader
{
  public:
    static std::optional<Model> loadGltf(const std::string &path);
};
} // namespace Cthulhu::Rendering