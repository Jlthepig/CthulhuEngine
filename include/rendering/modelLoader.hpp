#pragma once

#include <string>

#include "model.hpp"
namespace Cthulhu::Rendering
{
class ModelLoader
{
  public:
    static Model loadGltf(const std::string &path);
};
} // namespace Cthulhu::Rendering