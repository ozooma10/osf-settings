#pragma once

#include <filesystem>
#include <stdexcept>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace OSFSettings::Test
{
    // Allow reads while denying the delete sharing needed for atomic replacement.
    class FileLock
    {
    public:
        explicit FileLock(const std::filesystem::path& path) : m_handle(::CreateFileW(path.c_str(),
            GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr))
        {
            if (m_handle == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot lock persistence fixture");
        }
        ~FileLock() { Release(); }
        FileLock(const FileLock&) = delete;
        FileLock& operator=(const FileLock&) = delete;
        void Release() { if (m_handle != INVALID_HANDLE_VALUE) { ::CloseHandle(m_handle); m_handle = INVALID_HANDLE_VALUE; } }
    private:
        HANDLE m_handle;
    };
}
