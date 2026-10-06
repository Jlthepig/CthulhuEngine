#ifdef _WIN32

#include "platform.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Shobjidl.h>
#include <shellapi.h>
#include <windows.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

namespace
{
class RecycleOnlySink final : public IFileOperationProgressSink
{
  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
    {
        if (!object)
        {
            return E_POINTER;
        }

        if (riid == __uuidof(IUnknown) || riid == __uuidof(IFileOperationProgressSink))
        {
            *object = static_cast<IFileOperationProgressSink *>(this);
            AddRef();
            return S_OK;
        }

        *object = nullptr;
        return E_NOINTERFACE;
    }

    // these pretty much just live on the stack counting is alll these do
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++refs;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        return --refs;
    }

    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD flags, IShellItem *) override
    {
        if ((flags & static_cast<DWORD>(TSF_DELETE_RECYCLE_IF_POSSIBLE)) != 0)
        {
            return S_OK;
        }

        wouldDestroy = true;
        return E_ABORT;
    }

    HRESULT STDMETHODCALLTYPE StartOperations() override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT, IShellItem *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD, IShellItem *, HRESULT, IShellItem *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT, UINT) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResetTimer() override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PauseTimer() override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResumeTimer() override
    {
        return S_OK;
    }

    bool wouldDestroy = false;

  private:
    ULONG refs{1};
};
} // namespace

namespace Cthulhu::Core::Platform
{
bool moveToTrash(const std::filesystem::path &file)
{
    std::error_code error;
    std::filesystem::path full = std::filesystem::absolute(file, error).lexically_normal();
    if (error || !std::filesystem::exists(full, error))
    {
        return false;
    }
    full.make_preferred();

    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(init) && init != RPC_E_CHANGED_MODE)
    {
        return false;
    }

    const DWORD flags =
        static_cast<DWORD>(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOFX_RECYCLEONDELETE);

    bool ok = false;
    IFileOperation *operation = nullptr;
    IShellItem *item = nullptr;
    RecycleOnlySink sink;
    DWORD cookie{};

    if (SUCCEEDED(CoCreateInstance(__uuidof(FileOperation), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&operation))) &&
        SUCCEEDED(operation->SetOperationFlags(flags)) &&
        SUCCEEDED(SHCreateItemFromParsingName(full.c_str(), nullptr, IID_PPV_ARGS(&item))) &&
        SUCCEEDED(operation->Advise(&sink, &cookie)))
    {
        BOOL aborted = FALSE;
        ok = SUCCEEDED(operation->DeleteItem(item, nullptr)) && SUCCEEDED(operation->PerformOperations()) &&
             SUCCEEDED(operation->GetAnyOperationsAborted(&aborted)) && !aborted && !sink.wouldDestroy;

        operation->Unadvise(cookie);
    }

    if (item)
    {
        item->Release();
    }
    if (operation)
    {
        operation->Release();
    }
    if (SUCCEEDED(init))
    {
        CoUninitialize();
    }

    return ok && !std::filesystem::exists(full, error);
}
} // namespace Cthulhu::Core::Platform

#endif