#pragma once 

#include <filesystem>

namespace Cthulhu::Core::Platform
{
    // will move a file to the specific OS trash bin which is recycle bin on windows
    // each OS of course needs a different implementation but this function is a global one that will use them
    [[nodiscard]] bool moveToTrash(const std::filesystem::path& file);
}