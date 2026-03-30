#pragma once

#include "model.h"
#include <string>
namespace Cthulhu::Rendering
{
    class ModelLoader
    {
        public:
        static Model loadGltf(const std::string& path);
    };
}