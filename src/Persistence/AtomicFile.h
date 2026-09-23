#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace OSFSettings::Persistence
{
    bool WriteAtomic(const std::filesystem::path& path, std::string_view text, std::string& error);
}
