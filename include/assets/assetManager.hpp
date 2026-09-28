#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assetHandle.hpp"
#include "model.hpp"

namespace Cthulhu::Project
{
class Project;
}

namespace Cthulhu::Assets
{
    using ModelHandle = AssetHandle<Rendering::Model>;

    [[nodiscard]] std::optional<std::string> normaliseResourcePath(std::string_view resourcePath);

    class AssetManager
    {
        public:
            AssetManager() = default;
            ~AssetManager();

            AssetManager(const AssetManager &) = delete;
            AssetManager &operator=(const AssetManager &) = delete;

            void setProject(const Project::Project *activeProject) noexcept
            {
                project = activeProject;
            }

            [[nodiscard]] ModelHandle loadModel(std::string_view resourcePath);
            
            [[nodiscard]] Rendering::Model *getModel(ModelHandle handle);
            [[nodiscard]] const Rendering::Model *getModel(ModelHandle handle) const;

            [[nodiscard]] std::size_t getLoadedModelCount() const noexcept;

            void shutdown();

        private:
            struct ModelSlot
            {
                std::unique_ptr<Rendering::Model> model;
                std::string resourcePath;
                uint32_t generation{};
            };

            const Project::Project *project = nullptr;

            std::vector<ModelSlot> modelSlots;
            std::unordered_map<std::string, uint32_t> modelIndexByPath;
    };
}
