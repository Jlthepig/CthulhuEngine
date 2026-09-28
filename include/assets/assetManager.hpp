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

            // must pair a valid acquire with releaseModel();
            [[nodiscard]] ModelHandle acquireModel(std::string_view resourcePath);

            void releaseModel(ModelHandle handle);

            [[nodiscard]] uint32_t getModelRefCount(ModelHandle handle) const;
            [[nodiscard]] uint32_t getTotalModelRefCount() const noexcept;

            [[nodiscard]] std::size_t getLoadedModelCount() const noexcept;

            std::size_t collectUnusedModels();

            void shutdown();

        private:
            struct ModelSlot
            {
                std::unique_ptr<Rendering::Model> model;
                std::string resourcePath;
                uint32_t generation{};
                int32_t refCount{};
            };

            const Project::Project *project = nullptr;

            std::vector<ModelSlot> modelSlots;
            std::unordered_map<std::string, uint32_t> modelIndexByPath;
    };
}
