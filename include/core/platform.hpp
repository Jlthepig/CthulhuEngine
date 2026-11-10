#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

// <<<< global abstraction for per os implementations for misc actions >>>>
namespace Cthulhu::Core
{
class Window;
}
namespace Cthulhu::Core::Platform
{

enum class FileDialogStatus { Selected, Cancelled, Failed };

struct FileDialogFilter
{
    std::string_view name;
    std::string_view pattern;
};

struct FileDialogResult
{
    FileDialogStatus status = FileDialogStatus::Cancelled;
    std::filesystem::path file;
    std::string error;
};

[[nodiscard]] FileDialogResult openFileDialog(const Window &owner, std::span<const FileDialogFilter> filters);
[[nodiscard]] bool moveToTrash(const std::filesystem::path &file);

} // namespace Cthulhu::Core::Platform
