#pragma once

#include <cstdio>
#include <filesystem>
#include <functional>
#include <io.h>
#include <span>
#include <string>

extern "C" __declspec(dllimport) int __stdcall MoveFileExW(const wchar_t*, const wchar_t*, unsigned long);

namespace OSFSettings::Persistence
{
    using AtomicReplacer = std::function<bool(const std::filesystem::path&, const std::filesystem::path&)>;

    inline bool ReplaceFile(const std::filesystem::path& temporary, const std::filesystem::path& destination)
    {
        return ::MoveFileExW(temporary.c_str(), destination.c_str(), 0x1 | 0x8) != 0;
    }

    // Callers serialize writes to a path. Never remove a temporary file we did not open.
    inline bool WriteAtomic(const std::filesystem::path& path, std::span<const std::byte> bytes,
        std::string& error, const AtomicReplacer& replace = {})
    {
        error.clear();
        auto temporary = path;
        temporary += ".tmp";
        struct Cleanup {
            const std::filesystem::path& path;
            bool owned{};
            ~Cleanup() { if (owned) { std::error_code ignored; std::filesystem::remove(path, ignored); } }
        } cleanup{temporary};
        try {
            if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
            FILE* file{};
            if (_wfopen_s(&file, temporary.c_str(), L"wb") != 0 || !file) {
                error = "cannot open temporary state file";
                return false;
            }
            cleanup.owned = true;
            bool ok = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
            ok = ok && std::fflush(file) == 0 && _commit(_fileno(file)) == 0;
            ok = std::fclose(file) == 0 && ok;
            if (!ok) { error = "cannot finish writing state file"; return false; }
            if (!(replace ? replace(temporary, path) : ReplaceFile(temporary, path))) {
                error = "cannot replace state file";
                return false;
            }
            cleanup.owned = false;
            return true;
        } catch (const std::exception& exception) {
            error = exception.what();
            return false;
        }
    }
}
