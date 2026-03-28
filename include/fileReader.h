#pragma once

#include <string>
#include <fstream>

namespace Cthulhu::Utils
{
    
    using std::fstream;

    class FileReader
    {
        public:
        static std::string readFile(const std::string& file);

    };

}
