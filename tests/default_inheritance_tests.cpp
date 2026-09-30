#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

int TestDefaultInheritance()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        ++checks;
        if (!passed) throw std::runtime_error(std::string("Default inheritance: ") + message);
    };
    const auto root = std::filesystem::temp_directory_path() /
        ("osfsettings-defaults-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    } cleanup{ root };
    const auto schemas = root / "schemas";
    std::filesystem::create_directories(schemas);
    const auto write = [](const std::filesystem::path& path, const Json& document) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path);
        output << document.dump();
        if (!output) throw std::runtime_error("cannot write default inheritance fixture");
    };
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream input(path);
        return Json::parse(input);
    };
    const auto original = Json::parse(R"({"groups":{"General":[
        {"key":"enabled","type":"bool","default":true},
        {"key":"count","type":"int","default":3,"min":0,"max":10},
        {"key":"scale","type":"float","default":0.5},
        {"key":"mode","type":"enum","default":"a","options":["a","b"]},
        {"key":"binding","type":"key","default":115},
        {"key":"text","type":"string","default":"old"}
    ]}})");
    auto updated = original;
    const Json newDefaults = { false, 4, 0.75, "b", 255, "new" };
    for (std::size_t i = 0; i < newDefaults.size(); ++i) updated["groups"]["General"][i]["default"] = newDefaults[i];
    std::string error;
    const auto oldValues = SettingsJson::ParseSchema(original, "sample", error)->DefaultValues();
    const auto newValues = SettingsJson::ParseSchema(updated, "sample", error)->DefaultValues();
    write(schemas / "sample.json", original);

    SettingsStore untouched, pinned, reset, resetMod, legacy;
    untouched.LoadAll(schemas, root / "untouched");
    check(untouched.FindMod("sample")->values.empty() && !untouched.GetValue("sample", "unknown"),
        "fresh records are sparse and unknown settings remain absent");
    check(untouched.Reset("sample", "count").ok() && untouched.ResetMod("sample").ok() &&
        !std::filesystem::exists(root / "untouched"), "resetting inherited defaults does not create persistence");
    check(untouched.Set("sample", "count", std::int64_t{8}).ok(), "one explicit choice saves");
    check(read(root / "untouched/sample.json")["values"] == Json{{"count", 8}}, "neighbors remain absent from persistence");

    // Unchanged writes inherit defaults; changing away and back records a choice.
    pinned.LoadAll(schemas, root / "pinned");
    for (const auto& [key, value] : oldValues) {
        const auto result = pinned.Set("sample", key, value);
        check(result.ok() && !result.changed && pinned.GetValue("sample", key) == value &&
            pinned.FindMod("sample")->values.empty() && !std::filesystem::exists(root / "pinned"),
            "equal-value Set reads defaults without creating saved entries or files");
    }
    for (const auto& [key, value] : oldValues) {
        const auto edited = pinned.Set("sample", key, newValues.at(key));
        const auto restored = pinned.Set("sample", key, value);
        check(edited.ok() && edited.changed && restored.ok() && restored.changed,
            "changing away and back to a default saves the explicit choice");
    }
    const auto pinnedFile = read(root / "pinned/sample.json");
    check(pinnedFile["values"].size() == oldValues.size(), "all explicitly selected defaults are saved");
    write(root / "reset/sample.json", pinnedFile);
    reset.LoadAll(schemas, root / "reset");
    auto remaining = oldValues.size();
    for (const auto& [key, value] : oldValues) {
        const auto result = reset.Reset("sample", key);
        const auto saved = read(root / "reset/sample.json")["values"];
        --remaining;
        check(result.ok() && !result.changed && reset.GetValue("sample", key) == value &&
            !saved.contains(key) && saved.size() == remaining, "equal-value Reset removes only its override");
    }
    write(root / "reset-mod/sample.json", pinnedFile);
    resetMod.LoadAll(schemas, root / "reset-mod");
    const auto cleared = resetMod.ResetMod("sample");
    check(cleared.ok() && !cleared.changed && read(root / "reset-mod/sample.json")["values"] == Json::object(),
        "equal-value ResetMod removes every override");
    check(reset.FindMod("sample")->values.empty() && resetMod.FindMod("sample")->values.empty(),
        "resets clear saved entries in memory as well as on disk");

    // A pre-tracking full snapshot is indistinguishable from explicit choices.
    write(root / "legacy/sample.json", pinnedFile);
    legacy.LoadAll(schemas, root / "legacy");
    check(legacy.Set("sample", "count", std::int64_t{9}).ok() &&
        read(root / "legacy/sample.json")["values"].size() == oldValues.size(), "legacy equal-default entries survive another edit");

    write(schemas / "sample.json", updated);
    untouched.LoadAll(schemas, root / "untouched");
    pinned.LoadAll(schemas, root / "pinned");
    reset.LoadAll(schemas, root / "reset");
    resetMod.LoadAll(schemas, root / "reset-mod");
    legacy.LoadAll(schemas, root / "legacy");
    auto expected = newValues;
    expected["count"] = std::int64_t{8};
    check(untouched.LoadErrors().empty() && untouched.FindMod("sample")->ResolvedValues() == expected &&
        untouched.FindMod("sample")->values == SettingValues{{"count", std::int64_t{8}}},
        "untouched settings adopt updated defaults while the explicit choice survives");
    check(pinned.LoadErrors().empty() && pinned.FindMod("sample")->values == oldValues,
        "explicit choices equal to old defaults survive an author update for every type");
    check(reset.LoadErrors().empty() && reset.FindMod("sample")->ResolvedValues() == newValues &&
        resetMod.LoadErrors().empty() && resetMod.FindMod("sample")->ResolvedValues() == newValues,
        "single and mod resets inherit updated defaults after restart for every type");
    expected = oldValues;
    expected["count"] = std::int64_t{9};
    check(legacy.LoadErrors().empty() && legacy.FindMod("sample")->values == expected,
        "legacy files preserve all valid old values after an author update");

    // Equality with a newer default must not silently turn an override into inheritance.
    check(untouched.Set("sample", "scale", 0.5).ok() && untouched.Set("sample", "scale", 0.75).ok(),
        "setting back to the current default remains explicit");
    check(untouched.Set("sample", "enabled", true).ok() && untouched.Reset("sample", "enabled").ok() &&
        !read(root / "untouched/sample.json")["values"].contains("enabled"), "reset of a changed value removes its override");
    check(untouched.Set("sample", "count", std::int64_t{4}).ok(), "existing override can equal the new default");
    untouched.LoadAll(schemas, root / "untouched");
    check(untouched.Set("sample", "text", std::string("chosen")).ok() &&
        read(root / "untouched/sample.json")["values"]["count"] == 4, "saving after reload retains an override equal to the new default");
    write(schemas / "sample.json", original);
    untouched.LoadAll(schemas, root / "untouched");
    check(untouched.GetValue("sample", "count") == SettingValue{std::int64_t{4}} &&
        untouched.GetValue("sample", "scale") == SettingValue{0.75} && untouched.GetValue("sample", "enabled") == SettingValue{true},
        "explicit defaults stay pinned and reset values inherit across another schema update");
    check(untouched.ResetMod("sample").ok() && read(root / "untouched/sample.json")["values"] == Json::object(),
        "mod reset of changed values clears persistence");

    // Invalid/removed entries must not pin fallback defaults on the next save.
    write(root / "mixed/sample.json", Json{{"formatVersion", 1}, {"values", {
        {"enabled", "bad"}, {"count", 99}, {"mode", "removed"}, {"text", "kept"}, {"unknown", true}
    }}});
    SettingsStore mixed;
    mixed.LoadAll(schemas, root / "mixed");
    check(mixed.LoadErrors().size() == 3 && mixed.Set("sample", "scale", 0.25).ok(), "invalid saved entries fall back while valid edits can save");
    check(read(root / "mixed/sample.json")["values"] == Json{{"scale", 0.25}, {"text", "kept"}},
        "invalid and unknown entries do not persist their fallback defaults");
    write(schemas / "sample.json", updated);
    mixed.LoadAll(schemas, root / "mixed");
    expected = newValues;
    expected["scale"] = 0.25;
    expected["text"] = std::string("kept");
    check(mixed.LoadErrors().empty() && mixed.FindMod("sample")->ResolvedValues() == expected,
        "rejected saved entries inherit the next schema defaults");
    untouched.LoadAll(schemas, root / "untouched");
    check(untouched.FindMod("sample")->values.empty() && untouched.FindMod("sample")->ResolvedValues() == newValues,
        "reset-mod defaults update on next launch without populating saved values");
    return checks;
}
