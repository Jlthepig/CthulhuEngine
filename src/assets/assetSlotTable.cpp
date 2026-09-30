#include "assetSlotTable.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Assets
{
std::optional<AssetSlotTable::SlotId> AssetSlotTable::find(const std::string &key) const
{
    auto it = indexByKey.find(key);
    if (it == indexByKey.end())
    {
        return std::nullopt;
    }
    return SlotId{it->second, slots[it->second].generation};
}

AssetSlotTable::SlotId AssetSlotTable::allocate(const std::string &key)
{
    uint32_t index = 0;

    if (!freeSlots.empty())
    {
        index = freeSlots.back();
        freeSlots.pop_back();
    }
    else
    {
        index = static_cast<uint32_t>(slots.size());
        slots.emplace_back();
        slots.back().generation = 1;
    }

    auto &slot = slots[index];
    slot.key = key;
    slot.refCount = 0;
    slot.alive = true;

    indexByKey[key] = index;
    return SlotId{index, slot.generation};
}

bool AssetSlotTable::isAlive(uint32_t index, uint32_t generation) const noexcept
{
    return generation != 0 && index < slots.size() && slots[index].alive &&
           slots[index].generation == generation;
}

bool AssetSlotTable::addRef(uint32_t index, uint32_t generation) noexcept
{
    if (!isAlive(index, generation))
    {
        return false;
    }
    ++slots[index].refCount;
    return true;
}

void AssetSlotTable::release(uint32_t index, uint32_t generation)
{
    if (!isAlive(index, generation))
    {
        return;
    }

    auto &slot = slots[index];
    if (slot.refCount == 0)
    {
        Log::Print("ASSET RELEASED MORE TIMES THAN ACQUIRED: " + slot.key, "AssetSlotTable", LogType::LOG_ERROR);
        return;
    }
    --slot.refCount;
}

uint32_t AssetSlotTable::refCount(uint32_t index, uint32_t generation) const noexcept
{
    return isAlive(index, generation) ? slots[index].refCount : 0;
}

uint32_t AssetSlotTable::totalRefCount() const noexcept
{
    uint32_t total = 0;
    for (const auto &slot : slots)
    {
        if (slot.alive)
        {
            total += slot.refCount;
        }
    }
    return total;
}

std::size_t AssetSlotTable::liveCount() const noexcept
{
    return indexByKey.size();
}

std::vector<AssetSlotTable::Collected> AssetSlotTable::collectUnreferenced()
{
    std::vector<Collected> collected;

    for (uint32_t index = 0; index < static_cast<uint32_t>(slots.size()); ++index)
    {
        auto &slot = slots[index];
        if (!slot.alive || slot.refCount != 0)
        {
            continue;
        }

        collected.push_back({index, slot.key});
        indexByKey.erase(slot.key);

        slot.key.clear();
        slot.alive = false;

        ++slot.generation;
        if (slot.generation == 0)
        {
            slot.generation = 1;
        }

        freeSlots.push_back(index);
    }

    return collected;
}

void AssetSlotTable::clear()
{
    slots.clear();
    freeSlots.clear();
    indexByKey.clear();
}
} // namespace Cthulhu::Assets