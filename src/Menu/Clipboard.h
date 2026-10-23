#pragma once

#include <string_view>

namespace OSFSettings
{
    // Replaces the clipboard with UTF-8 text as CRLF Unicode text; false if Windows refuses.
    bool CopyToClipboard(std::string_view text);
}
