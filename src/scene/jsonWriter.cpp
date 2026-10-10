#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

#include "assetRegistry.hpp"
#include "components.hpp"
#include "jsonParser.hpp"
#include "jsonWriter.hpp"
#include "light.hpp"
#include "log_utils.hpp"
#include "scene.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Scene
{
namespace
{
std::string escapeJson(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\r':
            out += "\\r";
            break;
        default:
            if (static_cast<unsigned char>(c) >= 0x20)
                out += c;
        }
    }
    return out;
}

struct JsonWriter
{
    std::ostringstream oss;
    int depth = 0;
    int indentWidth = 4;
    std::vector<bool> firstStack;
    bool pretty = true;

    int writeIndent()
    {
        if (pretty)
        {
            oss << "\n" << std::string(depth * indentWidth, ' ');
            return depth * indentWidth + 1;
        }
        return 0;
    }

    void beginObject()
    {
        oss << "{";
        depth++;
        firstStack.push_back(true);
    }
    void endObject()
    {
        depth--;
        writeIndent();
        oss << "}";
        firstStack.pop_back();
    }
    void beginArray()
    {
        oss << "[";
        depth++;
        firstStack.push_back(true);
    }
    void endArray()
    {
        depth--;
        writeIndent();
        oss << "]";
        firstStack.pop_back();
    }

    void key(const std::string &k)
    {
        if (!firstStack.back())
        {
            oss << ",";
            writeIndent();
        }
        else
            writeIndent();
        firstStack.back() = false;
        oss << "\"" << escapeJson(k) << "\": ";
    }
    void value(float v)
    {
        oss << std::setprecision(std::numeric_limits<float>::max_digits10) << v;
    }
    void value(int v)
    {
        oss << v;
    }
    void value(bool b)
    {
        oss << (b ? "true" : "false");
    }
    void value(const std::string &s)
    {
        oss << "\"" << escapeJson(s) << "\"";
    }

    void commaArr()
    {
        if (!firstStack.back())
        {
            oss << ",";
            writeIndent();
        }
        firstStack.back() = false;
    }

    void vec3Value(const glm::vec3 &v)
    {
        beginArray();
        value(v.x);
        oss << ", ";
        value(v.y);
        oss << ", ";
        value(v.z);
        endArray();
    }

    void vec3(const std::string &k, const glm::vec3 &v)
    {
        key(k);
        vec3Value(v);
    }
};

bool writeField(JsonWriter &w, const FieldDescriptor &field, const FieldValue &value,
                const Assets::AssetRegistry &registry)
{
    w.key(field.name);

    switch (field.type)
    {
    case FieldType::Float:
        w.value(std::get<float>(value));
        return true;
    case FieldType::Int:
        w.value(std::get<int>(value));
        return true;
    case FieldType::Bool:
        w.value(std::get<bool>(value));
        return true;
    case FieldType::Vec3:
    case FieldType::Color:
        w.vec3Value(std::get<glm::vec3>(value));
        return true;
    case FieldType::String:
        w.value(std::get<std::string>(value));
        return true;
    case FieldType::Enum:
    {
        const int index = std::get<int>(value);
        if (index < 0 || index >= static_cast<int>(field.enumNames.size()))
        {
            Log::Print("CANNOT SAVE ENUM VALUE OUT OF RANGE: " + field.name, "SceneWriter", LogType::LOG_ERROR);
            w.value(std::string{});
            return false;
        }
        w.value(field.enumNames[static_cast<size_t>(index)]);
        return true;
    }
    case FieldType::AssetRef:
    {
        const auto &path = std::get<std::string>(value);
        w.beginObject();
        w.key("path");
        w.value(path);
        if (const auto *record = registry.findByPath(path))
        {
            w.key("id");
            w.value(Assets::assetIdToString(record->id));
        }
        w.endObject();
        return true;
    }
    case FieldType::EntityRef:
        w.value(entityIdToString(std::get<EntityId>(value)));
        return true;
    }
    return false;
}
} // namespace

bool SceneWriter::writeScene(const Scene &scene, const std::string &path, const Assets::AssetRegistry &registry)
{
    JsonWriter w;
    w.beginObject();

    w.key("format_version");
    w.value(static_cast<int>(SCENE_FORMAT_VERSION));

    w.key("name");
    w.value(scene.getName());

    w.key("entities");
    w.beginArray();

    bool valid = true;
    const ComponentRegistry &components = *scene.getComponentRegistry();

    std::unordered_set<EntityId, EntityIdHash> writtenIds;
    scene.getWorld().each([&](flecs::entity e, const EntityIdentityComponent &identity, const NameComponent &name) {
        if (!identity.id.isValid())
        {
            Log::Print("CANNOT SAVE ENTITY WITH INVALID ID", "SceneWriter", LogType::LOG_ERROR);
            valid = false;
            return;
        }

        if (!writtenIds.insert(identity.id).second)
        {
            Log::Print("CANNOT SAVE SCENE WITH DUPLICATE ENTITY IDs", "SceneWriter", LogType::LOG_ERROR);
            valid = false;
            return;
        }

        w.commaArr();
        w.beginObject();

        w.key("id");
        w.value(entityIdToString(identity.id));

        w.key("name");
        w.value(name.name);

        auto parentId = scene.getParent(identity.id);
        if (parentId)
        {
            if (!scene.isEntityAlive(*parentId))
            {
                valid = false;
                return;
            }

            w.key("parent");
            w.value(entityIdToString(*parentId));
        }

        w.key("components");
        w.beginObject();
        for (const auto &descriptor : components.getAll())
        {
            if (!descriptor.has(e))
            {
                continue;
            }

            w.key(descriptor.name);
            w.beginObject();
            for (const auto &field : descriptor.fields)
            {
                if (!writeField(w, field, field.get(e), registry))
                {
                    valid = false;
                }
            }
            w.endObject();
        }
        w.endObject();

        w.endObject();
    });

    if (!valid)
    {
        return false;
    }

    w.endArray();

    const auto &dir = scene.getDirectionalLight();
    w.key("directional_light");
    w.beginObject();
    w.vec3("direction", dir.direction);
    w.vec3("color", dir.color);
    w.key("intensity");
    w.value(dir.intensity);
    w.endObject();

    w.key("point_lights");
    w.beginArray();
    for (const auto &pl : scene.getPointLights())
    {
        w.commaArr();
        w.beginObject();
        w.vec3("position", pl.position);
        w.vec3("color", pl.color);
        w.key("intensity");
        w.value(pl.intensity);
        w.key("radius");
        w.value(pl.radius);
        w.key("constant");
        w.value(pl.constant);
        w.key("linear");
        w.value(pl.linear);
        w.key("quadratic");
        w.value(pl.quadratic);
        w.endObject();
    }
    w.endArray();

    w.endObject();

    std::ofstream out(path);
    if (!out.is_open())
    {
        Log::Print("FAILED TO OPEN SCENE FILE FOR WRITING: " + path, "SceneWriter", LogType::LOG_ERROR);
        return false;
    }
    out << w.oss.str();

    out.flush();
    if (!out.good())
    {
        Log::Print("FAILED WHILE WRITING SCENE FILE: " + path, "SceneWriter", LogType::LOG_ERROR);
        return false;
    }

    Log::Print("Scene written: " + path, "SceneWriter", LogType::LOG_SUCCESS);
    return true;
}
} // namespace Cthulhu::Scene
