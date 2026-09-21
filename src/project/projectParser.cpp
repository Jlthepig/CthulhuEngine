#include "projectParser.h"
#include "log_utils.hpp"

#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;
namespace Cthulhu::Project
{
    static std::string_view trim(std::string_view text)
    {
        while (!text.empty() &&
            std::isspace(static_cast<unsigned char>(text.front())))
        {
            text.remove_prefix(1);
        }

        while (!text.empty() &&
            std::isspace(static_cast<unsigned char>(text.back())))
        {
            text.remove_suffix(1);
        }

        return text;
    }

    static std::string_view stripComment(std::string_view line)
    {
        bool insideString = false;
        bool escaping = false;

        for (size_t i = 0; i < line.size(); ++i)
        {
            const char c = line[i];

            if (escaping)
            {
                escaping = false;
                continue;
            }

            if (insideString && c == '\\')
            {
                escaping = true;
                continue;
            }

            if (c == '"')
            {
                insideString = !insideString;
                continue;
            }

            if (c == '#' && !insideString)
            {
                return line.substr(0, i);
            }
        }

        return line;
    }

    static bool parseString(std::string_view value, std::string& output)
    {
        value = trim(value);

        if (value.size() < 2 ||
            value.front() != '"' ||
            value.back() != '"')
        {
            return false;
        }

        value.remove_prefix(1);
        value.remove_suffix(1);

        std::string result;
        result.reserve(value.size());

        bool escaping = false;

        for (char c : value)
        {
            if (escaping)
            {
                switch (c)
                {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case 'n':  result += '\n'; break;
                    case 't':  result += '\t'; break;
                    default:   return false;
                }

                escaping = false;
                continue;
            }

            if (c == '\\')
            {
                escaping = true;
                continue;
            }

            if (c == '"')
            {
                return false;
            }

            result += c;
        }

        if (escaping)
        {
            return false;
        }

        output = std::move(result);
        return true;
    }

    static bool parseUInt32(std::string_view value, uint32_t& output)
    {
        value = trim(value);

        if (value.empty())
        {
            return false;
        }

        uint32_t parsed = 0;

        const char* begin = value.data();
        const char* end = value.data() + value.size();

        auto result = std::from_chars(begin, end, parsed);

        if (result.ec != std::errc{} ||
            result.ptr != end)
        {
            return false;
        }

        output = parsed;
        return true;
    }

    enum class ProjectKey
    {
        Name,
        WindowWidth,
        WindowHeight,
        MainScene,
        Unknown
    };

    static ProjectKey classifyKey(std::string_view key)
    {
        if (key == "name")         return ProjectKey::Name;
        if (key == "windowWidth")  return ProjectKey::WindowWidth;
        if (key == "windowHeight") return ProjectKey::WindowHeight;
        if (key == "mainScene")    return ProjectKey::MainScene;
        return ProjectKey::Unknown;
    }

    static bool parseWindowDimension(std::string_view keyName, std::string_view value, size_t lineNumber, bool alreadyFound, uint32_t& output)
    {
        if (alreadyFound)
        {
            Log::Print("DUPLICATE PROJECT SETTING '" + std::string(keyName) + "' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
            return false;
        }

        uint32_t parsed = 0;

        uint32_t maxWindowDimensions = ProjectParser::getMaxWindowDimension();

        if (!parseUInt32(value, parsed) || parsed == 0 || parsed > maxWindowDimensions)
        {
            Log::Print("INVALID '" + std::string(keyName) + "' ON LINE " + std::to_string(lineNumber) + " (EXPECTED 1-" + std::to_string(maxWindowDimensions) + ")", "ProjectParser", LogType::LOG_ERROR);
            return false;
        }

        output = parsed;
        return true;
    }

    std::optional<ProjectConfig> ProjectParser::parse(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            Log::Print("FAILED TO OPEN PROJECT FILE: " + path, "ProjectParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        ProjectConfig config;

        bool foundName = false;
        bool foundWindowWidth = false;
        bool foundWindowHeight = false;
        bool foundMainScene = false;

        std::string line;
        size_t lineNumber = 0;

        while (std::getline(file, line))
        {
            ++lineNumber;

            std::string_view view = line;

            if (lineNumber == 1 &&
                view.size() >= 3 &&
                static_cast<unsigned char>(view[0]) == 0xEF &&
                static_cast<unsigned char>(view[1]) == 0xBB &&
                static_cast<unsigned char>(view[2]) == 0xBF)
            {
                view.remove_prefix(3);
            }

            view = stripComment(view);
            view = trim(view);

            if (view.empty())
            {
                continue;
            }

            const size_t equalsPosition = view.find('=');

            if (equalsPosition == std::string_view::npos)
            {
                Log::Print("EXPECTED '=' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                return std::nullopt;
            }

            const std::string_view key = trim(view.substr(0, equalsPosition));

            const std::string_view value =trim(view.substr(equalsPosition + 1));

            if (key.empty() || value.empty())
            {
                Log::Print("INVALID PROJECT SETTING ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                return std::nullopt;
            }

            switch (classifyKey(key))
            {
                case ProjectKey::Name:
                {
                    if (foundName)
                    {
                        Log::Print("DUPLICATE PROJECT SETTING 'name' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                        return std::nullopt;
                    }

                    std::string name;

                    if (!parseString(value, name))
                    {
                        Log::Print("INVALID STRING FOR 'name' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                        return std::nullopt;
                    }

                    config.name = std::move(name);
                    foundName = true;
                    break;
                }

                case ProjectKey::WindowWidth:
                {
                    if (!parseWindowDimension("windowWidth", value, lineNumber,foundWindowWidth,config.windowWidth))
                    {
                        return std::nullopt;
                    }

                    foundWindowWidth = true;
                    break;
                }

                case ProjectKey::WindowHeight:
                {
                    if (!parseWindowDimension("windowHeight", value, lineNumber,foundWindowHeight, config.windowHeight))
                    {
                        return std::nullopt;
                    }

                    foundWindowHeight = true;
                    break;
                }

                case ProjectKey::MainScene:
                {
                    if (foundMainScene)
                    {
                        Log::Print("DUPLICATE PROJECT SETTING 'mainScene' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                        return std::nullopt;
                    }

                    std::string mainScene;

                    if (!parseString(value, mainScene))
                    {
                        Log::Print("INVALID STRING FOR 'mainScene' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                        return std::nullopt;
                    }

                    if (mainScene.empty())
                    {
                        Log::Print("'mainScene' CANNOT BE EMPTY. REMOVE THE SETTING IF NO MAIN SCENE IS CONFIGURED.", "ProjectParser", LogType::LOG_ERROR);
                        return std::nullopt;
                    }

                    if (!mainScene.starts_with("res://"))
                    {
                        Log::Print("'mainScene' MUST USE A 'res://' RESOURCE PATH", "ProjectParser", LogType::LOG_ERROR);
                        return std::nullopt;
                    }

                    config.mainScene = std::move(mainScene);
                    foundMainScene = true;
                    break;
                }

                case ProjectKey::Unknown:
                default:
                {
                    Log::Print("UNKNOWN PROJECT SETTING '" + std::string(key) + "' ON LINE " + std::to_string(lineNumber), "ProjectParser", LogType::LOG_ERROR);
                    return std::nullopt;
                }
            }
        }

        if (!foundName || !foundWindowWidth || !foundWindowHeight)
        {
            std::string missing;

            if (!foundName)
            {
                missing += " name";
            }

            if (!foundWindowWidth)
            {
                missing += " windowWidth";
            }

            if (!foundWindowHeight)
            {
                missing += " windowHeight";
            }

            Log::Print("PROJECT FILE IS MISSING REQUIRED SETTINGS:" + missing, "ProjectParser", LogType::LOG_ERROR);
            return std::nullopt;
        }

        return config;
    }
}