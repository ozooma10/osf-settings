#include "AtomicFile.h"

#include <cerrno>
#include <fstream>
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

        auto temporary = path;
        temporary += ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            code.assign(errno, std::generic_category());
            error = temporary.string() + ": cannot open temporary file: " + code.message();
            return false;
        }
        output << text;
        output.close();
        if (!output) {
            error = temporary.string() + ": cannot finish writing temporary file";
        } else if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            code.assign(static_cast<int>(::GetLastError()), std::system_category());
            error = path.string() + ": cannot replace file: " + code.message();
        } else {
            return true;
        }
        std::filesystem::remove(temporary, code);
        return false;
    }
}
