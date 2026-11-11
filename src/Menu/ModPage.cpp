#include "ModPage.h"
#include <Windows.h>
#include <comdef.h>
#include <exdisp.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <format>

namespace OSFSettings
{
    namespace
    {
        // Run the URL through Explorer's own Shell.Application so the browser becomes Explorer's child.
        // Launching Edge directly from Starfield inherits USVFS hooks and can crash it.
        // https://devblogs.microsoft.com/oldnewthing/20131118-00/?p=2643
        HRESULT ShellExecuteFromDesktop(const wchar_t* url)
        {
            using Microsoft::WRL::ComPtr;
            ComPtr<IShellWindows> windows;
            ComPtr<IDispatch> desktop, viewDispatch, application;
            ComPtr<IShellBrowser> browser;
            ComPtr<IShellView> view;
            ComPtr<IShellFolderViewDual> folder;
            ComPtr<IShellDispatch2> shell;
            _variant_t location(static_cast<long>(CSIDL_DESKTOP)), empty;
            long desktopHandle{};

            HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&windows));
            if (SUCCEEDED(hr)) hr = windows->FindWindowSW(&location, &empty, SWC_DESKTOP, &desktopHandle, SWFO_NEEDDISPATCH, &desktop);
            if (SUCCEEDED(hr) && !desktop) hr = E_FAIL;
            if (SUCCEEDED(hr)) hr = IUnknown_QueryService(desktop.Get(), SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
            if (SUCCEEDED(hr)) hr = browser->QueryActiveShellView(&view);
            if (SUCCEEDED(hr)) hr = view->GetItemObject(SVGIO_BACKGROUND, IID_PPV_ARGS(&viewDispatch));
            if (SUCCEEDED(hr)) hr = viewDispatch.As(&folder);
            if (SUCCEEDED(hr)) hr = folder->get_Application(&application);
            if (SUCCEEDED(hr)) hr = application.As(&shell);
            if (SUCCEEDED(hr)) hr = shell->ShellExecute(_bstr_t(url), empty, empty, empty, _variant_t(static_cast<long>(SW_SHOWNORMAL)));
            return hr;
        }
    }

    std::int32_t OpenModPage(std::uint32_t nexusModId)
    {
        if (!nexusModId) return E_INVALIDARG;
        const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(init) && init != RPC_E_CHANGED_MODE) return init;
        const auto result = ShellExecuteFromDesktop(std::format(L"https://www.nexusmods.com/starfield/mods/{}", nexusModId).c_str());
        if (SUCCEEDED(init)) CoUninitialize();
        return result;
    }
}
