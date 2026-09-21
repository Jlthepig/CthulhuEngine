#include "applicationPaths.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#elif defined(__linux__)

#include <array>
#include <unistd.h>

#endif

namespace Cthulhu::Core {
std::optional<std::filesystem::path> getExecutableDirectory() {
#if defined(_WIN32)

  std::wstring buffer(32768, L'\0');

  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()));

  if (length == 0 || length >= buffer.size()) {
    return std::nullopt;
  }

  buffer.resize(length);

  return std::filesystem::path(buffer).parent_path();

#elif defined(__linux__)

  std::array<char, 4096> buffer{};

  const ssize_t length =
      readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);

  if (length <= 0) {
    return std::nullopt;
  }

  return std::filesystem::path(
             std::string(buffer.data(), static_cast<size_t>(length)))
      .parent_path();

#else

  return std::nullopt;

#endif
}
} // namespace Cthulhu::Core