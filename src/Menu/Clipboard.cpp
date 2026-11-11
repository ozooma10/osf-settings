#include "Clipboard.h"
#include <Windows.h>
#include <string>

namespace OSFSettings
{
    bool CopyToClipboard(std::string_view text)
    {
        // Older Windows editors show bare LF text as one line.
        std::string lines;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) lines += '\r';
            lines += text[i];
        }
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, lines.data(), static_cast<int>(lines.size()), nullptr, 0);
        if (length <= 0) return false;
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (static_cast<std::size_t>(length) + 1) * sizeof(wchar_t));
        if (!memory) return false;
        auto* buffer = static_cast<wchar_t*>(GlobalLock(memory));
        MultiByteToWideChar(CP_UTF8, 0, lines.data(), static_cast<int>(lines.size()), buffer, length);
        buffer[length] = L'\0';
        GlobalUnlock(memory);

        // SetClipboardData fails when the clipboard is emptied without an owner window.
        // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-emptyclipboard
        HWND owner = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
        bool copied = false;
        if (owner && OpenClipboard(owner)) {
            copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
            CloseClipboard();
        }
        if (owner) DestroyWindow(owner);
        // The clipboard owns the memory only after SetClipboardData succeeds.
        if (!copied) GlobalFree(memory);
        return copied;
    }
}
