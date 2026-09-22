#pragma once

#include <fstream>
#include <string>

namespace Cthulhu::Utils
{

using std::fstream;

class FileReader
{
  public:
    static std::string readFile(const std::string &file);
};

} // namespace Cthulhu::Utils
