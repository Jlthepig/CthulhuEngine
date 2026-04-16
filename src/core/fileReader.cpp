#include "fileReader.h"
#include "log_utils.hpp"

#include <sstream>

using KalaHeaders::KalaLog::Log; 
using KalaHeaders::KalaLog::LogType; 

namespace Cthulhu::Utils
{
    std::string FileReader::readFile(const std::string& file)
    {
        std::ifstream input(file);

        if (!input.is_open())
        {
            Log::Print("FILE MISSING DOUBLE CHECK PATH!","File",LogType::LOG_ERROR);
            return "";
        }

        std::stringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }
}