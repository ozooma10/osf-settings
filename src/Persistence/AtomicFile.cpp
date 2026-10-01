#include "AtomicFile.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace OSFSettings::Persistence
{
    bool WriteAtomic(const std::filesystem::path& path, std::string_view text, std::string& error)
    {
        error.clear();
        std::error_code code;
        const auto directory = path.parent_path();
        if (!directory.empty()) {
            std::filesystem::create_directories(directory, code);
            if (code) {
                error = directory.string() + ": cannot create directory: " + code.message();
                return false;
            }
        }

        // CREATE_NEW never follows or truncates a pre-existing temporary file
        // (including a hard link). Each write owns exactly the file it created.
        static std::atomic_uint64_t next{};
        std::filesystem::path temporary;
        HANDLE output = INVALID_HANDLE_VALUE;
        for (unsigned attempt = 0; attempt < 128; ++attempt) {
            temporary = path;
            temporary += ".tmp." + std::to_string(::GetCurrentProcessId()) + "." + std::to_string(++next);
            output = ::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (output != INVALID_HANDLE_VALUE) break;
            const auto failure = ::GetLastError();
            if (failure != ERROR_FILE_EXISTS && failure != ERROR_ALREADY_EXISTS) {
                code.assign(static_cast<int>(failure), std::system_category());
                error = temporary.string() + ": cannot create temporary file: " + code.message();
                return false;
            }
        }
        if (output == INVALID_HANDLE_VALUE) {
            error = path.string() + ": cannot allocate an unused temporary file";
            return false;
        }
        struct Cleanup {
            HANDLE handle;
            const std::filesystem::path& path;
            bool remove{true};
            ~Cleanup() { if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle); if (remove) ::DeleteFileW(path.c_str()); }
        } cleanup{output, temporary};
        while (!text.empty()) {
            const auto size = static_cast<DWORD>(std::min<std::size_t>(text.size(), 1024 * 1024));
            DWORD written{};
            if (!::WriteFile(output, text.data(), size, &written, nullptr) || !written) {
                code.assign(static_cast<int>(::GetLastError()), std::system_category());
                error = temporary.string() + ": cannot write temporary file: " + code.message();
                return false;
            }
            text.remove_prefix(written);
        }
        if (!::FlushFileBuffers(output)) {
            code.assign(static_cast<int>(::GetLastError()), std::system_category());
            error = temporary.string() + ": cannot flush temporary file: " + code.message();
            return false;
        }
        ::CloseHandle(output);
        cleanup.handle = INVALID_HANDLE_VALUE;
        if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            code.assign(static_cast<int>(::GetLastError()), std::system_category());
            error = path.string() + ": cannot replace file: " + code.message();
        } else {
            cleanup.remove = false;
            return true;
        }
        return false;
    }
}
