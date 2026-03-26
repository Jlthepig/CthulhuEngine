#pragma once

#include <string>
#include <fstream>

namespace Cthulhu::Utils
{
    using std::string;
    using std::fstream;

    class FileReader
    {
        public:
        static string readFile(const string& file);

    };

}
