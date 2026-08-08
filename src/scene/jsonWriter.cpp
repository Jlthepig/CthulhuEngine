
#include "jsonWriter.h"
#include "scene.h"
#include "components.h"
#include "light.h"
#include "log_utils.hpp"
#include <sstream>
#include <iomanip>
#include <fstream>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

namespace Cthulhu::Scene
{
    namespace
    {
        std::string escapeJson(const std::string& s)
        {
            std::string out; out.reserve(s.size());
            for (char c : s)
            {
                switch (c)
                {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\t': out += "\\t";  break;
                    case '\r': out += "\\r";  break;
                    default:
                        if (static_cast<unsigned char>(c) >= 0x20) out += c;
                }
            }
            return out;
        }

        struct JsonWriter
        {
            std::ostringstream oss;
            int depth = 0; int indentWidth = 4;
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

            void beginObject() { oss << "{"; depth++; firstStack.push_back(true); }
            void endObject() { depth--; writeIndent(); oss << "}"; firstStack.pop_back(); }
            void beginArray()  { oss << "["; depth++; firstStack.push_back(true); }
            void endArray()  { depth--; writeIndent(); oss << "]"; firstStack.pop_back(); }

            void key(const std::string& k) {
                if (!firstStack.back()) {oss << ","; writeIndent();}
                else writeIndent();
                firstStack.back() = false;
                oss << "\"" << escapeJson(k) << "\": ";
            }
            void value(float v)  { oss << std::setprecision(7) << v; }
            void value(int v)    { oss << v; }
            void value(bool b)   { oss << (b ? "true" : "false"); }
            void value(const std::string& s) { oss << "\"" << escapeJson(s) << "\""; }

            void commaArr() { if (!firstStack.back()) {oss << ","; writeIndent();} firstStack.back() = false; }

            void vec3(const std::string& k, const glm::vec3& v) {
                key(k); beginArray();
                value(v.x); oss << ", "; value(v.y); oss << ", "; value(v.z);
                endArray();
            }
        };
    }

    bool SceneWriter::writeScene(const Scene& scene, const std::string& path)
    {
        JsonWriter w;
        w.beginObject();

        w.key("name"); w.value(scene.getName());

        w.key("entities"); w.beginArray();
        scene.getWorld().each([&](flecs::entity e, const TransformComponent& transform)
        {
            w.commaArr();
            w.beginObject();

            w.key("name"); w.value(std::string(e.name()));
            w.vec3("position", transform.position);
            w.vec3("rotation", transform.rotation);
            w.vec3("scale",    transform.scale);

            if (e.has<MeshComponent>())
            {
                const auto& m = e.get<MeshComponent>();
                w.key("model"); w.value(m.modelPath);
                w.key("bounds"); w.beginObject();
                w.vec3("min", m.boundsMin);
                w.vec3("max", m.boundsMax);
                w.endObject();
            }

            if (e.has<PhysicsComponent>())
            {
                const auto& p = e.get<PhysicsComponent>();
                if (p.hasBody)
                {
                    w.key("physics"); w.beginObject();
                    w.key("type");        w.value(p.type);
                    w.vec3("half_extent", p.halfExtent);
                    w.key("mass");        w.value(p.mass);
                    w.endObject();
                }
            }

            if (e.has<WeaponComponent>())               // Gap B
            {
                const auto& wp = e.get<WeaponComponent>();
                w.key("weapon"); w.beginObject();
                w.key("firerate"); w.value(wp.firerate);
                w.key("maxrange"); w.value(wp.maxRange);
                w.endObject();
            }

            if (e.has<AudioSourceComponent>())          // Gap B
            {
                const auto& au = e.get<AudioSourceComponent>();
                w.key("audio"); w.beginObject();
                w.key("file");   w.value(au.filePath);
                w.key("volume"); w.value(au.volume);
                w.key("loop");   w.value(au.loop);
                w.endObject();
            }

            if (e.has<TagPlayer>())                     // Gap B
            {
                w.key("player"); w.value(true);
            }

            w.endObject();
        });
        w.endArray();

        const auto& dir = scene.getDirectionalLight();
        w.key("directional_light"); w.beginObject();
        w.vec3("direction", dir.direction);
        w.vec3("color",     dir.color);
        w.key("intensity"); w.value(dir.intensity);
        w.endObject();

        w.key("point_lights"); w.beginArray();
        for (const auto& pl : scene.getPointLights())
        {
            w.commaArr();
            w.beginObject();
            w.vec3("position",  pl.position);
            w.vec3("color",     pl.color);
            w.key("intensity"); w.value(pl.intensity);
            w.key("constant");  w.value(pl.constant);
            w.key("linear");    w.value(pl.linear);
            w.key("quadratic"); w.value(pl.quadratic);
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
        Log::Print("Scene written: " + path, "SceneWriter", LogType::LOG_SUCCESS);
        return true;
    }
}
