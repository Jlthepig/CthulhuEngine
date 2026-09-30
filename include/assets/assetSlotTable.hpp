#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Cthulhu::Assets
{
    
class AssetSlotTable
{
  public:
    struct SlotId
    {
        uint32_t index = 0;
        uint32_t generation = 0;
    };

    struct Collected
    {
        uint32_t index = 0;
        std::string key;
    };

    [[nodiscard]] std::optional<SlotId> find(const std::string &key) const;

    [[nodiscard]] SlotId allocate(const std::string &key);

    [[nodiscard]] bool isAlive(uint32_t index, uint32_t generation) const noexcept;

    bool addRef(uint32_t index, uint32_t generation) noexcept;
    void release(uint32_t index, uint32_t generation); 

    [[nodiscard]] uint32_t refCount(uint32_t index, uint32_t generation) const noexcept;
    [[nodiscard]] uint32_t totalRefCount() const noexcept;
    [[nodiscard]] std::size_t liveCount() const noexcept;

    [[nodiscard]] std::vector<Collected> collectUnreferenced();

    void clear();

  private:
    struct Slot
    {
        std::string key;
        uint32_t generation = 0;
        uint32_t refCount = 0;
        bool alive = false;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlots;
    std::unordered_map<std::string, uint32_t> indexByKey;
};
} // namespace Cthulhu::Assets