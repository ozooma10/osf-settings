#include "FileLock.h"
#include "Persistence/AtomicFile.h"
#include "Settings/SettingsJson.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view) { return std::nullopt; }
    bool IsBindableKey(std::uint32_t) { return false; }
}

int main()
{
    using namespace OSFSettings;
    namespace fs = std::filesystem;
    int checks{};
    const auto check = [&](bool ok, const char* message) {
        ++checks;
        if (!ok) throw std::runtime_error(message);
    };
    const auto read = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    const auto write = [](const fs::path& path, const std::string& text) {
        std::ofstream output(path, std::ios::binary); output << text;
    };
    try {
        std::string error;
        for (const auto* id : {"con", "prn", "aux", "nul", "com0", "com1", "com9", "lpt0", "lpt1", "lpt9", "nul.extra", "com1.more.parts"}) {
            std::istringstream input("{\"groups\":{}}");
            check(!SettingsJson::ParseSchema(input, id, error), "device name must not become a mod filename");
        }
        for (const auto* id : {"console", "null", "com10", "lpt10", "my.mod", "my-nul"})
            check(IsValidModId(id), "ordinary identifiers remain valid");
        check(IsValidModId(std::string(128, 'x')) && !IsValidModId(std::string(129, 'x')), "mod id length boundary");

        const auto root = fs::current_path() / "build" / "tests" / "security" /
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        fs::create_directories(root / "values");
        const auto victim = root / "unrelated.txt";
        const auto saved = root / "values" / "audit.json";
        write(victim, "preserve unrelated content");
        fs::create_hard_link(victim, saved);
        check(Persistence::WriteAtomic(saved, "second", error) && read(saved) == "second" && read(victim) == "preserve unrelated content",
            "replacement changes destination link without truncating its target");
        {
            Test::FileLock lock(saved);
            check(!Persistence::WriteAtomic(saved, "blocked", error) && !error.empty() && read(saved) == "second",
                "replacement failure preserves existing contents");
            check(std::distance(fs::directory_iterator(root / "values"), fs::directory_iterator{}) == 1,
                "replacement failure removes its temporary file");
        }
        check(Persistence::WriteAtomic(saved, "retry", error) && read(saved) == "retry", "failed save can be retried");
        check(Persistence::WriteAtomic(root / "empty.json", "", error) && fs::file_size(root / "empty.json") == 0, "empty writes succeed");
        const std::string large(2 * 1024 * 1024 + 1, 'x');
        check(Persistence::WriteAtomic(root / "large.json", large, error) && read(root / "large.json") == large, "multi-chunk writes are complete");

        std::cout << checks << " security checks passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
