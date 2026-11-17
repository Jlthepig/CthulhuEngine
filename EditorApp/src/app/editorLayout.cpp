#include "editorLayout.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

#include "log_utils.hpp"

namespace Cthulhu::Editor
{
namespace
{
constexpr bool valid(const EditorLayout &layout)
{
    for (float ratio : layout.ratios)
        if (!(ratio > 0.0f && ratio < 1.0f))
            return false;

    return true;
}

static_assert(valid(EditorLayout{}));

void warning(std::string_view message)
{
    KalaHeaders::KalaLog::Log::Print(
        std::string(message), "Editor Layout",
        KalaHeaders::KalaLog::LogType::LOG_WARNING);
}

std::filesystem::path layoutFile()
{
#ifdef _WIN32
    const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!size) return {};

    std::wstring buffer(size, L'\0');
    const DWORD written =
        GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), size);
    if (!written || written >= size) return {};

    buffer.resize(written);
    return std::filesystem::path(buffer) / L"Cthulhu" / L"editor-layout.txt";
#else
    if (const char *config = std::getenv("XDG_CONFIG_HOME"); config && *config)
        return std::filesystem::path(config) / "Cthulhu" / "editor-layout.txt";

    if (const char *home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".config" / "Cthulhu" /
            "editor-layout.txt";

    return {};
#endif
}

std::optional<EditorLayout> readLayout(std::istream &input)
{
    input.imbue(std::locale::classic());

    std::string magic;
    unsigned version{};
    EditorLayout layout;
    int hierarchy{}, inspector{}, browser{};

    if (!(input >> magic >> version) ||
        magic != "CthulhuLayout" || version != 1)
        return std::nullopt;

    for (float &ratio : layout.ratios)
        if (!(input >> ratio)) return std::nullopt;

    if (!(input >> hierarchy >> inspector >> browser) ||
        (hierarchy != 0 && hierarchy != 1) ||
        (inspector != 0 && inspector != 1) ||
        (browser != 0 && browser != 1) ||
        !valid(layout))
        return std::nullopt;

    input >> std::ws;
    if (!input.eof()) return std::nullopt;

    layout.hierarchy = hierarchy != 0;
    layout.inspector = inspector != 0;
    layout.browser = browser != 0;
    return layout;
}

void writeLayout(std::ostream &output, const EditorLayout &layout)
{
    output.imbue(std::locale::classic());
    output << "CthulhuLayout 1\n"
           << std::setprecision(std::numeric_limits<float>::max_digits10);

    for (float ratio : layout.ratios)
        output << ratio << ' ';

    output << '\n'
           << layout.hierarchy << ' '
           << layout.inspector << ' '
           << layout.browser << '\n';
}

#ifndef NDEBUG
void checkLayoutCodec()
{
    EditorLayout expected;
    expected.ratios = {0.77f, 0.61f, 0.27f, 0.33f};
    expected.hierarchy = false;
    expected.browser = false;

    std::stringstream roundTrip;
    writeLayout(roundTrip, expected);
    assert(readLayout(roundTrip) == expected);

    for (const char *text : {
        "", "CthulhuLayout 2", "CthulhuLayout 1 0.8",
        "CthulhuLayout 1 0.8 0.7 0.2 0.2 2 1 1",
        "CthulhuLayout 1 0 0.7 0.2 0.2 1 1 1",
        "CthulhuLayout 1 1 0.7 0.2 0.2 1 1 1",
        "CthulhuLayout 1 nan 0.7 0.2 0.2 1 1 1",
        "CthulhuLayout 1 0.8 0.7 0.2 0.2 1 1 1 extra"})
    {
        std::istringstream invalid(text);
        assert(!readLayout(invalid));
    }
}
#endif
}

EditorLayout loadEditorLayout()
{
#ifndef NDEBUG
    checkLayoutCodec();
#endif

    const auto file = layoutFile();
    if (file.empty())
    {
        warning("Could not locate editor settings. Using the default layout.");
        return {};
    }

    std::error_code error;
    const auto size = std::filesystem::file_size(file, error);

    if (error == std::errc::no_such_file_or_directory)
        return {};

    if (error || size > 4096)
    {
        warning("Could not read editor layout settings. Using the default layout.");
        return {};
    }

    std::ifstream input(file);
    const auto layout = readLayout(input);
    if (layout) return *layout;

    warning("Invalid editor layout settings. Using the default layout.");
    return {};
}

void saveEditorLayout(const EditorLayout &layout)
{
    const auto file = layoutFile();
    if (file.empty() || !valid(layout))
    {
        warning("Could not save editor layout settings.");
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error)
    {
        warning("Could not create the editor settings folder.");
        return;
    }

    auto temporary = file;
#ifdef _WIN32
    temporary += "." + std::to_string(GetCurrentProcessId()) + ".tmp";
#else
    temporary += "." + std::to_string(getpid()) + ".tmp";
#endif

    std::ofstream output(temporary, std::ios::trunc);
    writeLayout(output, layout);
    output.flush();
    const bool written = output.good();
    output.close();

    bool saved = written && !output.fail();
    if (saved)
    {
#ifdef _WIN32
        saved = MoveFileExW(
            temporary.c_str(), file.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#else
        std::filesystem::rename(temporary, file, error);
        saved = !error;
#endif
    }

    if (!saved)
    {
        std::filesystem::remove(temporary, error);
        warning("Could not save the editor layout. Previous settings were retained.");
    }
}
}