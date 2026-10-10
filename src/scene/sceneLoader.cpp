#include <algorithm>
#include <cmath>

#include "sceneLoader.hpp"
#include "assetRegistry.hpp"
#include "components.hpp"
#include "jsonParser.hpp"
#include "project.hpp"
#include "log_utils.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
namespace
{
std::string resolveReference(const Assets::AssetRegistry &registry, const std::optional<Assets::AssetId> &id,
                             const std::string &pathHint)
{
    if (!id)
    {
        return pathHint;
    }

    if (const auto *record = registry.findById(*id))
    {
        if (record->path != pathHint)
        {
            Log::Print("Asset moved: " + pathHint + " -> " + record->path, "SceneLoader", LogType::LOG_INFO);
        }
        return record->path;
    }

    Log::Print("UNKNOWN ASSET ID, FALLING BACK TO PATH: " + pathHint, "SceneLoader", LogType::LOG_WARNING);
    return pathHint;
}

std::optional<FieldValue> toFieldValue(const FieldDescriptor &field, const ParsedValue &value,
                                       const Assets::AssetRegistry &assets)
{
    const auto *number = std::get_if<double>(&value);
    const auto *text = std::get_if<std::string>(&value);

    switch (field.type)
    {
    case FieldType::Float:
        if (number)
        {
            return FieldValue{static_cast<float>(*number)};
        }
        return std::nullopt;
    case FieldType::Int:
        if (number && std::floor(*number) == *number)
        {
            return FieldValue{static_cast<int>(*number)};
        }
        return std::nullopt;
    case FieldType::Bool:
        if (const auto *flag = std::get_if<bool>(&value))
        {
            return FieldValue{*flag};
        }
        return std::nullopt;
    case FieldType::Vec3:
    case FieldType::Color:
        if (const auto *vector = std::get_if<glm::vec3>(&value))
        {
            return FieldValue{*vector};
        }
        return std::nullopt;
    case FieldType::String:
        if (text)
        {
            return FieldValue{*text};
        }
        return std::nullopt;
    case FieldType::Enum:
        if (text)
        {
            for (size_t i = 0; i < field.enumNames.size(); ++i)
            {
                if (field.enumNames[i] == *text)
                {
                    return FieldValue{static_cast<int>(i)};
                }
            }
        }
        return std::nullopt;
    case FieldType::AssetRef:
        if (const auto *ref = std::get_if<ParsedAssetRef>(&value))
        {
            return FieldValue{resolveReference(assets, ref->id, ref->path)};
        }
        if (text)
        {
            return FieldValue{*text};
        }
        return std::nullopt;
    case FieldType::EntityRef:
        if (text)
        {
            if (auto id = entityIdFromString(*text))
            {
                return FieldValue{*id};
            }
        }
        return std::nullopt;
    }
    return std::nullopt;
}

bool applyComponents(flecs::entity entity, const ParsedEntity &parsed, const ComponentRegistry &components,
                     const Assets::AssetRegistry &assets)
{
    for (const auto &parsedComponent : parsed.components)
    {
        if (!components.find(parsedComponent.name))
        {
            Log::Print("UNKNOWN COMPONENT SKIPPED (lost if saved): " + parsedComponent.name + " on " + parsed.name,"SceneLoader", LogType::LOG_WARNING);
        }
    }

    // registration order
    for (const auto &descriptor : components.getAll())
    {
        const auto found = std::find_if(parsed.components.begin(), parsed.components.end(),
                                        [&](const ParsedComponent &c) { return c.name == descriptor.name; });
        if (found == parsed.components.end())
        {
            continue;
        }

        ComponentSnapshot snapshot{descriptor.name, {}};
        for (const auto &parsedField : found->fields)
        {
            const FieldDescriptor *field = findField(descriptor, parsedField.name);
            if (!field)
            {
                Log::Print("UNKNOWN FIELD IGNORED: " + descriptor.name + "." + parsedField.name + " on " + parsed.name,
                           "SceneLoader", LogType::LOG_WARNING);
                continue;
            }

            auto value = toFieldValue(*field, parsedField.value, assets);
            if (!value)
            {
                Log::Print("INVALID VALUE FOR " + descriptor.name + "." + field->name + " on " + parsed.name,
                           "SceneLoader", LogType::LOG_ERROR);
                return false;
            }

            snapshot.fields.push_back({field->name, std::move(*value)});
        }

        if (!components.apply(entity, {snapshot}))
        {
            return false;
        }
    }

    return true;
}
} // namespace

bool SceneLoader::load(const std::string &path, Scene &scene, [[maybe_unused]] const Cthulhu::Project::Project &project,
                       const Assets::AssetRegistry &registry)
{
    auto parsed = JsonParser::parseScene(path); // the parsed information provided by the json parser
    if (!parsed.has_value())
    {
        Log::Print("FAILED TO LOAD SCENE: " + path, "SceneLoader", LogType::LOG_ERROR);
        return false;
    }

    scene.setName(parsed->name);

    Log::Print("Loading scene: " + parsed->name, "SceneLoader", LogType::LOG_INFO);

    // build entities from parsed information
    for (auto &parsedEntity : parsed->entities)
    {
        auto entity = scene.createEntityWithId(parsedEntity.id, parsedEntity.name);
        if (!entity)
        {
            Log::Print("FAILED TO CREATE ENTITY FROM SCENE: " + entityIdToString(parsedEntity.id), "SceneLoader",
                       LogType::LOG_ERROR);
            return false;
        }

        if (!applyComponents(*entity, parsedEntity, *scene.getComponentRegistry(), registry))
        {
            Log::Print("FAILED TO LOAD COMPONENTS OF: " + parsedEntity.name, "SceneLoader", LogType::LOG_ERROR);
            return false;
        }
    }

    for (const auto &parsedEntity : parsed->entities)
    {
        if (!parsedEntity.parentId)
        {
            continue;
        }

        if (!scene.isEntityAlive(*parsedEntity.parentId))
        {
            Log::Print("ENTITY REFERENCES MISSING PARENT: " + entityIdToString(*parsedEntity.parentId), "SceneLoader",
                       LogType::LOG_ERROR);
            return false;
        }

        if (!scene.setParent(parsedEntity.id, *parsedEntity.parentId))
        {
            Log::Print("FAILED TO RESTORE ENTITY HIERARCHY", "SceneLoader", LogType::LOG_ERROR);
            return false;
        }
    }

    scene.setDirectionalLight(parsed->directionalLight);
    for (auto &pl : parsed->pointLights) // for every light in scene/parsed
                                         // information  add a light
    {
        scene.addPointLight(pl);
    }

    return true;
}
} // namespace Cthulhu::Scene