#include "assetId.hpp"
#include "entityId.hpp"

namespace Cthulhu::Assets
{

// will refactor later as there is duplicated logic from entityId.cpp

AssetId generateAssetId()
{
    const auto id = Scene::generateEntityId();
    return AssetId{id.high, id.low};
}

std::string assetIdToString(AssetId id)
{
    return Scene::entityIdToString(Scene::EntityId{id.high, id.low});
}

std::optional<AssetId> assetIdFromString(std::string_view value)
{
    const auto parsed = Scene::entityIdFromString(value);
    if (!parsed)
    {
        return std::nullopt;
    }
    return AssetId{parsed->high, parsed->low};
}
} // namespace Cthulhu::Assets