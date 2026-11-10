#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <Shobjidl.h>
#include <wrl/client.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <cassert>
#include <chrono>
#include <exception>
#include <future>
#include <system_error>
#include <thread>

#include "platform.hpp"
#include "window.hpp"
#include "log_utils.hpp"

#pragma comment(lib, "ole32.lib")

// <<<< Cthulhus WINDOWS file dialog implementation >>>>

namespace
{
using namespace Cthulhu::Core::Platform;

struct ComScope
{
    HRESULT status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    ~ComScope()
    {
        if (SUCCEEDED(status)) CoUninitialize();
    }
};

FileDialogResult failed(HRESULT error)
{
    KalaHeaders::KalaLog::Log::Print("FILE DIALOG FAILED, HRESULT: " + std::to_string(static_cast<unsigned long>(error)),
        "Platform", KalaHeaders::KalaLog::LogType::LOG_ERROR);

    return {FileDialogStatus::Failed, {}, "Could not open the file picker. See the log for details."};
}

std::optional<std::wstring> toWide(std::string_view text)
{
    if (text.empty() || text.find('\0') != std::string_view::npos || text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return std::nullopt;

    const int size = static_cast<int>(text.size());
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
    if (count <= 0) return std::nullopt;

    std::wstring wide(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, wide.data(), count) != count)
        return std::nullopt;

    return wide;
}
FileDialogResult showFileDialog(
    HWND parent, std::span<const FileDialogFilter> filters)
{
    assert(GetCurrentThreadId() !=
           GetWindowThreadProcessId(parent, nullptr));

    if (filters.size() > std::numeric_limits<UINT>::max())
        return failed(E_INVALIDARG);

    std::vector<std::wstring> names, patterns;
    std::vector<COMDLG_FILTERSPEC> nativeFilters;
    names.reserve(filters.size());
    patterns.reserve(filters.size());
    nativeFilters.reserve(filters.size());

    for (const auto &filter : filters)
    {
        auto name = toWide(filter.name);
        auto pattern = toWide(filter.pattern);
        if (!name || !pattern) return failed(E_INVALIDARG);

        names.push_back(std::move(*name));
        patterns.push_back(std::move(*pattern));
        nativeFilters.push_back({names.back().c_str(), patterns.back().c_str()
        });
    }

    ComScope com;
    if (FAILED(com.status)) return failed(com.status);

    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(__uuidof(FileOpenDialog), nullptr, 
                                      CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(dialog.GetAddressOf()));
                                      
    if (FAILED(result)) return failed(result);

    DWORD options{};
    result = dialog->GetOptions(&options);
    if (FAILED(result)) return failed(result);

    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR;
    options &= ~static_cast<DWORD>(FOS_ALLOWMULTISELECT);

    result = dialog->SetOptions(options);
    if (FAILED(result)) return failed(result);

    if (!nativeFilters.empty())
    {
        result = dialog->SetFileTypes(static_cast<UINT>(nativeFilters.size()), nativeFilters.data());
        if (FAILED(result)) return failed(result);

        result = dialog->SetFileTypeIndex(1);
        if (FAILED(result)) return failed(result);
    }

    result = dialog->Show(parent);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    if (FAILED(result)) return failed(result);

    Microsoft::WRL::ComPtr<IShellItem> item;
    result = dialog->GetResult(item.GetAddressOf());
    if (FAILED(result)) return failed(result);

    PWSTR rawPath = nullptr;
    result = item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path( rawPath, &CoTaskMemFree);

    if (FAILED(result)) return failed(result);
    if (!path || *path.get() == L'\0') return failed(E_UNEXPECTED);

    std::filesystem::path file(path.get());
    if (!file.is_absolute()) return failed(E_UNEXPECTED);

    return {FileDialogStatus::Selected, std::move(file), {}};
}
} // namespace

namespace Cthulhu::Core::Platform
{
FileDialogResult openFileDialog(
    const Window &owner, std::span<const FileDialogFilter> filters)
{
    if (!owner.getWindow()) return failed(E_INVALIDARG);

    const HWND parent = glfwGetWin32Window(owner.getWindow());
    if (!parent) return failed(E_INVALIDARG);

    if (GetWindowThreadProcessId(parent, nullptr) != GetCurrentThreadId())
        return failed(RPC_E_WRONG_THREAD);

    std::promise<FileDialogResult> completion;
    auto outcome = completion.get_future();
    std::jthread worker;

    try
    {
        worker = std::jthread([parent, filters, &completion] {
            try
            {
                completion.set_value(showFileDialog(parent, filters));
            }
            catch (...)
            {
                completion.set_exception(std::current_exception());
            }
        });
    }
    catch (const std::system_error &)
    {
        return failed(E_FAIL);
    }

    bool quit = false;
    int quitCode = 0;

    // The picker synchronously messages its owner while opening and closing.
    while (outcome.wait_for(std::chrono::milliseconds(10)) !=
           std::future_status::ready)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                quit = true;
                quitCode = static_cast<int>(message.wParam);
            }
            else
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }

    worker.join();
    if (quit) PostQuitMessage(quitCode);

    return outcome.get();
}
} // namespace Cthulhu::Core::Platform

#endif