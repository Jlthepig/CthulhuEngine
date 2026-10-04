#include "componentRegistry.hpp"

#include <algorithm>
#include <cmath>

#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scene
{
namespace
{
    bool validateField(const ComponentDescriptor& component, const FieldDescriptor& field)
    {
        const std::string where = component.name + "." + field.name;

        if (field.name.empty() || !field.get || !field.set)
        {
            Log::Print("FIELD NEEDS A NAME GETTER AND SETTER: " + where, "Components", LogType::LOG_ERROR);
            return false;
        }

        if (field.type == FieldType::Enum && field.enumNames.empty())
        {
            Log::Print("ENUM FIELD HAS NO NAMES: " + where, "Components", LogType::LOG_ERROR);
            return false;
        }

        if (field.type == FieldType::AssetRef && field.assetType == Assets::AssetType::Unknown)
        {
            Log::Print("ASSET FIELD HAS NO ASSET TYPE: " + where, "Components", LogType::LOG_ERROR);
            return false;
        }

        if (field.min > field.max)
        {
            Log::Print("FIELD MIN IS ABOVE MAX: " + where, "Components", LogType::LOG_ERROR);
            return false;
        }

        return true;
    }

    bool isFinite(const glm::vec3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    std::optional<FieldValue> sanitize(const FieldDescriptor& field, const FieldValue& value)
    {
        if (!fieldValueMatches(field.type, value))
        {
            return std::nullopt;
        }

        const bool hasLimits = field.min < field.max;

        if (field.type == FieldType::Float)
        {
            float f = std::get<float>(value);
            if (!std::isfinite(f))
            {
                return std::nullopt;
            }
            return FieldValue(hasLimits ? std::clamp(f, field.min, field.max) : f);
        }

        if (field.type == FieldType::Int)
        {
            int i = std::get<int>(value);
            if (!hasLimits)
            {
                return value;
            }
            return FieldValue{std::clamp(i, static_cast<int>(field.min), static_cast<int>(field.max))};
        }

        if (field.type == FieldType::Enum)
        {
            const int index = std::get<int>(value);
            if (index < 0 || index >= static_cast<int>(field.enumNames.size()))
            {
                return std::nullopt;
            }
            return value;
        }

        if ((field.type == FieldType::Vec3 || field.type == FieldType::Color) && !isFinite(std::get<glm::vec3>(value)))
        {
            return std::nullopt;
        }

        return value;
    }
} // namespace

    bool fieldValueMatches(FieldType type, const FieldValue& value)
    {
        switch (type)
        {
            case FieldType::Float:
                return std::holds_alternative<float>(value);
            case FieldType::Int:
            case FieldType::Enum:
                return std::holds_alternative<int>(value);
            case FieldType::Bool:
                return std::holds_alternative<bool>(value);
            case FieldType::Vec3:
            case FieldType::Color:
                return std::holds_alternative<glm::vec3>(value);
            case FieldType::String:
            case FieldType::AssetRef:
                return std::holds_alternative<std::string>(value);
            case FieldType::EntityRef:
                return std::holds_alternative<EntityId>(value);
        }
        return false;
    }

    const FieldDescriptor* findField(const ComponentDescriptor& component, std::string_view field)
    {
        for (const auto& f : component.fields)
        {
            if (f.name == field)
            {
                return &f;
            }
        }
        return nullptr;
    }

    bool ComponentRegistry::registerComponent(ComponentDescriptor descriptor)
    {
        if (descriptor.name.empty() || !descriptor.has)
        {
            Log::Print("COMPONENT NEEDS A NAME AND A HAS FUNCTION", "Components", LogType::LOG_ERROR);
            return false;
        }

        if (!descriptor.core && (!descriptor.add || !descriptor.remove))
        {
            Log::Print("NON-CORE COMPONENT NEEDS ADD AND REMOVE: " + descriptor.name, "Components", LogType::LOG_ERROR);
            return false;
        }

        if (find(descriptor.name))
        {
            Log::Print("COMPONENT ALREADY REGISTERED: " + descriptor.name, "Components", LogType::LOG_ERROR);
            return false;
        }

        for (size_t i = 0; i < descriptor.fields.size(); ++i)
        {
            if (!validateField(descriptor, descriptor.fields[i]))
            {
                return false;
            }

            for (size_t j = 0; j < i; ++j)
            {
                if (descriptor.fields[j].name == descriptor.fields[i].name)
                {
                    Log::Print("DUPLICATE FIELD: " + descriptor.name + "." + descriptor.fields[i].name, "Components",
                            LogType::LOG_ERROR);
                    return false;
                }
            }
        }

        descriptors.push_back(std::move(descriptor));
        return true;
    }

    const ComponentDescriptor* ComponentRegistry::find(std::string_view name) const
    {
        for (const auto& d : descriptors)
        {
            if (d.name == name)
            {
                return &d;
            }
        }
        return nullptr;
    }

    std::optional<FieldValue> ComponentRegistry::getField(flecs::entity entity, std::string_view component,
                                                        std::string_view field) const
    {
        const ComponentDescriptor* descriptor = find(component);
        if (!descriptor || !entity.is_alive() || !descriptor->has(entity))
        {
            return std::nullopt;
        }

        const FieldDescriptor* fieldDescriptor = findField(*descriptor, field);
        if (!fieldDescriptor)
        {
            return std::nullopt;
        }

        return fieldDescriptor->get(entity);
    }

    bool ComponentRegistry::setField(flecs::entity entity, std::string_view component, std::string_view field,
                                    const FieldValue& value) const
    {
        const ComponentDescriptor* descriptor = find(component);
        if (!descriptor || !entity.is_alive() || !descriptor->has(entity))
        {
            return false;
        }

        const FieldDescriptor* fieldDescriptor = findField(*descriptor, field);
        if (!fieldDescriptor)
        {
            return false;
        }

        const auto clean = sanitize(*fieldDescriptor, value);
        if (!clean)
        {
            return false;
        }

        fieldDescriptor->set(entity, *clean);
        return true;
    }
} // namespace Cthulhu::Scene