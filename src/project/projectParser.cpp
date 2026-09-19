#include "projectParser.h"
#include "log_utils.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <string_view>

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

        for (size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '"' &&
                (i == 0 || line[i - 1] != '\\'))
            {
                insideString = !insideString;
            }

            if (line[i] == '#' && !insideString)
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

        output.clear();
        output.reserve(value.size());

        bool escaping = false;

        for (char c : value)
        {
            if (escaping)
            {
                switch (c)
                {
                    case '"':
                        output += '"';
                        break;

                    case '\\':
                        output += '\\';
                        break;

                    case 'n':
                        output += '\n';
                        break;

                    case 't':
                        output += '\t';
                        break;

                    default:
                        return false;
                }

                escaping = false;
                continue;
            }

            if (c == '\\')
            {
                escaping = true;
                continue;
            }

            output += c;
        }

        return !escaping;
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

    std::optional<ProjectConfig> ProjectParser::parse(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open())
        {
            KalaHeaders::KalaLog::Log::Print(
                "FAILED TO OPEN PROJECT FILE: " + path,
                "ProjectParser",
                KalaHeaders::KalaLog::LogType::LOG_ERROR);

            return std::nullopt;
        }

        ProjectConfig config;

        bool foundName = false;
        bool foundWindowWidth = false;
        bool foundWindowHeight = false;

        std::string line;
        size_t lineNumber = 0;

        while (std::getline(file, line))
        {
            ++lineNumber;

            std::string_view view = stripComment(line);
            view = trim(view);

            if (view.empty())
            {
                continue;
            }

            size_t equalsPosition = view.find('=');

            if (equalsPosition == std::string_view::npos)
            {
                KalaHeaders::KalaLog::Log::Print(
                    "EXPECTED '=' ON LINE " + std::to_string(lineNumber),
                    "ProjectParser",
                    KalaHeaders::KalaLog::LogType::LOG_ERROR);

                return std::nullopt;
            }

            std::string_view key =
                trim(view.substr(0, equalsPosition));

            std::string_view value =
                trim(view.substr(equalsPosition + 1));

            if (key.empty() || value.empty())
            {
                KalaHeaders::KalaLog::Log::Print(
                    "INVALID PROJECT SETTING ON LINE " +
                        std::to_string(lineNumber),
                    "ProjectParser",
                    KalaHeaders::KalaLog::LogType::LOG_ERROR);

                return std::nullopt;
            }

            if (key == "name")
            {
                if (foundName)
                {
                    KalaHeaders::KalaLog::Log::Print(
                        "DUPLICATE PROJECT SETTING 'name'",
                        "ProjectParser",
                        KalaHeaders::KalaLog::LogType::LOG_ERROR);

                    return std::nullopt;
                }

                if (!parseString(value, config.name))
                {
                    KalaHeaders::KalaLog::Log::Print(
                        "INVALID STRING FOR 'name' ON LINE " +
                            std::to_string(lineNumber),
                        "ProjectParser",
                        KalaHeaders::KalaLog::LogType::LOG_ERROR);

                    return std::nullopt;
                }

                foundName = true;
            }
            else if (key == "windowWidth")
            {
                if (foundWindowWidth ||
                    !parseUInt32(value, config.windowWidth) ||
                    config.windowWidth == 0)
                {
                    KalaHeaders::KalaLog::Log::Print(
                        "INVALID 'windowWidth' ON LINE " +
                            std::to_string(lineNumber),
                        "ProjectParser",
                        KalaHeaders::KalaLog::LogType::LOG_ERROR);

                    return std::nullopt;
                }

                foundWindowWidth = true;
            }
            else if (key == "windowHeight")
            {
                if (foundWindowHeight ||
                    !parseUInt32(value, config.windowHeight) ||
                    config.windowHeight == 0)
                {
                    KalaHeaders::KalaLog::Log::Print(
                        "INVALID 'windowHeight' ON LINE " +
                            std::to_string(lineNumber),
                        "ProjectParser",
                        KalaHeaders::KalaLog::LogType::LOG_ERROR);

                    return std::nullopt;
                }

                foundWindowHeight = true;
            }

            else
            {
                KalaHeaders::KalaLog::Log::Print(
                    "UNKNOWN PROJECT SETTING '" +
                        std::string(key) +
                        "' ON LINE " +
                        std::to_string(lineNumber),
                    "ProjectParser",
                    KalaHeaders::KalaLog::LogType::LOG_ERROR);

                return std::nullopt;
            }
        }

        if (!foundName || !foundWindowWidth || !foundWindowHeight)
        {
            KalaHeaders::KalaLog::Log::Print(
                "PROJECT FILE IS MISSING REQUIRED SETTINGS",
                "ProjectParser",
                KalaHeaders::KalaLog::LogType::LOG_ERROR);

            return std::nullopt;
        }

        return config;
    }
}