#pragma once

#include "projectConfig.h"

#include <optional>
#include <string>

namespace Cthulhu::Project
{
    class ProjectParser
    {
        public:
            static std::optional<ProjectConfig> parse(const std::string& path);

            static constexpr uint32_t getMaxWindowDimension()
            {
                return MAX_WINDOW_DIMENSION;
            }
            
        private:
            static constexpr uint32_t MAX_WINDOW_DIMENSION = 16384;

    };
}