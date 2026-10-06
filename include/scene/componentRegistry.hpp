#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <flecs.h>
#include <glm.hpp>

#include "assetType.hpp"
#include "entityId.hpp"

namespace Cthulhu::Scene
{

enum class FieldType : uint8_t
{
    Float,
    Int,
    Bool,
    Vec3,
    Color,
    String,
    Enum,
    AssetRef,
    EntityRef
};

using FieldValue = std::variant<float, int, bool, glm::vec3, std::string, EntityId>;

// changes how the editor will show a value << never ever what is stored >>
enum class FieldHint : uint8_t
{
    None,
    Degrees
};

using FieldGetter = FieldValue (*)(flecs::entity entity);
using FieldSetter = void (*)(flecs::entity entity, const FieldValue &value);

struct FieldDescriptor
{
    std::string name;
    FieldType type = FieldType::Float;
    FieldGetter get = nullptr;
    FieldSetter set = nullptr;

    float min = 0.0f;
    float max = 0.0f;
    float step = 0.0f;
    FieldHint hint = FieldHint::None;

    std::vector<std::string> enumNames;
    Assets::AssetType assetType = Assets::AssetType::Unknown;
};

struct ComponentDescriptor
{
    std::string name;

    bool (*has)(flecs::entity entity) = nullptr;
    void (*add)(flecs::entity entity) = nullptr;
    void (*remove)(flecs::entity entity) = nullptr;

    bool core = false;

    std::vector<FieldDescriptor> fields;
};
struct FieldSnapshot
{
    std::string field;
    FieldValue value;
};

struct ComponentSnapshot
{
    std::string component;
    std::vector<FieldSnapshot> fields;
};
class ComponentRegistry
{
  public:
    bool registerComponent(ComponentDescriptor descriptor);

    [[nodiscard]] const ComponentDescriptor *find(std::string_view name) const;

    [[nodiscard]] const std::vector<ComponentDescriptor> &getAll() const noexcept
    {
        return descriptors;
    }

    [[nodiscard]] std::optional<FieldValue> getField(flecs::entity entity, std::string_view component,
                                                     std::string_view field) const;

    bool setField(flecs::entity entity, std::string_view component, std::string_view field,
                  const FieldValue &value) const;

    [[nodiscard]] std::vector<ComponentSnapshot> capture(flecs::entity entity) const;

    [[nodiscard]] std::optional<ComponentSnapshot> captureComponent(flecs::entity entity, std::string_view) const;

    bool apply(flecs::entity entity, const std::vector<ComponentSnapshot> &components) const;

  private:
    std::vector<ComponentDescriptor> descriptors;
};

[[nodiscard]] const FieldDescriptor *findField(const ComponentDescriptor &component, std::string_view field);
[[nodiscard]] bool fieldValueMatches(FieldType type, const FieldValue &value);

[[nodiscard]] FieldDescriptor makeField(std::string name, FieldType type, FieldGetter get, FieldSetter set,
                                        float min = 0.0f, float max = 0.0f, float step = 0.0f);

template <typename T> [[nodiscard]] ComponentDescriptor makeComponentDescriptor(std::string name)
{
    ComponentDescriptor d;
    d.name = std::move(name);
    d.has = [](flecs::entity e) { return e.has<T>(); };
    d.add = [](flecs::entity e) {
        if constexpr (std::is_empty_v<T>)
        {
            e.add<T>();
        }
        else
        {
            e.set(T{});
        }
    };
    d.remove = [](flecs::entity e) { e.remove<T>(); };
    return d;
}

} // namespace Cthulhu::Scene