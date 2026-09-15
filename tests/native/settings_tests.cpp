#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace
{
    namespace fs = std::filesystem;
    using Json = nlohmann::json;
    using OSFSettings::SettingValue;
    int checks{};
    int failures{};

    void Check(bool condition, const char* message)
    {
        ++checks;
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void Reject(const Json& document, std::string_view expectedError)
    {
        std::string error;
        const auto schema = OSFSettings::SettingsJson::ParseSchema(document, error);
        ++checks;
        if (schema || error.find(expectedError) == std::string::npos) {
            ++failures;
            std::cerr << "FAIL: expected rejection containing '" << expectedError << "', got '" << error << "'\n";
        }
    }

    void Write(const fs::path& path, const std::string& text)
    {
        std::ofstream output(path);
        output << text;
        output.close();
        if (!output) throw std::runtime_error("could not write test fixture: " + path.string());
    }

    std::string Read(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("could not read test fixture: " + path.string());
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }

    void TestSchema(const Json& example)
    {
        std::string error = "old error";
        const auto schema = OSFSettings::SettingsJson::ParseSchema(example, error);
        Check(schema.has_value() && error.empty(), "the shipped example parses and clears the error");
        if (!schema) return;
        Check(schema->id == "learning" && schema->groups.size() == 1, "mod and group are loaded");
        const auto* setting = schema->FindSetting("notifications");
        const auto* boolean = setting ? std::get_if<OSFSettings::BoolDefinition>(&setting->definition) : nullptr;
        Check(boolean && boolean->defaultValue && setting->label == "Enable notifications",
            "boolean definition, default, and label are loaded");
        Check(schema->FindSetting("unknown") == nullptr, "unknown definition is absent");

        auto document = example;
        document["groups"][0]["settings"][0]["default"] = false;
        auto parsed = OSFSettings::SettingsJson::ParseSchema(document, error);
        Check(parsed && !std::get<OSFSettings::BoolDefinition>(parsed->groups[0].settings[0].definition).defaultValue, "false is a valid default");

        document = example;
        document.erase("title");
        document.erase("description");
        document["groups"][0].erase("label");
        document["groups"][0]["settings"][0].erase("label");
        document["groups"][0]["settings"][0].erase("hint");
        parsed = OSFSettings::SettingsJson::ParseSchema(document, error);
        Check(parsed && parsed->title == "learning" && parsed->description.empty() &&
            parsed->groups[0].label == "general" && parsed->groups[0].settings[0].label == "notifications" &&
            parsed->groups[0].settings[0].hint.empty(), "optional display text uses readable defaults");

        Reject(Json::array(), "schema must be an object");
        for (const auto& version : { Json(2), Json(1.0), Json("1"), Json(true), Json(nullptr) }) {
            document = example;
            document["schemaVersion"] = version;
            Reject(document, "schemaVersion");
        }
        document = example;
        document.erase("schemaVersion");
        Reject(document, "schemaVersion");
        document = example;
        document["id"] = "../learning";
        Reject(document, "mod id");
        document = example;
        document["groups"] = Json::object();
        Reject(document, "groups must be an array");
        document = example;
        document["groups"][0]["settings"] = Json::object();
        Reject(document, "settings must be an array");

        for (const auto* type : { "float", "enum", "string", "key", "flags", "action", "note" }) {
            document = example;
            document["groups"][0]["settings"][0]["type"] = type;
            Reject(document, "only types bool and int");
        }
        for (const auto& value : { Json("true"), Json(1), Json(nullptr) }) {
            document = example;
            document["groups"][0]["settings"][0]["default"] = value;
            Reject(document, "default must be a boolean");
        }
        document = example;
        document["groups"][0]["settings"][0].erase("default");
        Reject(document, "default must be a boolean");
        document = example;
        document["groups"][0]["settings"][0]["label"] = 12;
        Reject(document, "label must be a string");

        document = example;
        auto duplicate = document["groups"][0];
        document["groups"].push_back(duplicate);
        Reject(document, "duplicate group id");
        document["groups"][1]["id"] = "second";
        Reject(document, "duplicate setting key");
    }

    void TestIntegers(const Json& example)
    {
        auto schema = example;
        schema["groups"][0]["settings"].push_back({ { "key", "notificationLimit" }, { "type", "int" },
            { "label", "Notification limit" }, { "default", 3 }, { "min", 1 }, { "max", 10 } });
        schema["groups"][0]["settings"].push_back({ { "key", "counter" }, { "type", "int" }, { "default", 0 } });
        std::string error;
        const auto parsed = OSFSettings::SettingsJson::ParseSchema(schema, error);
        Check(parsed.has_value() && error.empty(), "a schema can mix booleans and integers");
        if (!parsed) return;
        const auto* limit = parsed->FindSetting("notificationLimit");
        const auto* integer = limit ? std::get_if<OSFSettings::IntDefinition>(&limit->definition) : nullptr;
        Check(integer && integer->defaultValue == 3 && integer->minimum == 1 && integer->maximum == 10,
            "integer definition, default, and inclusive bounds are loaded");

        const std::vector<Json> invalidDefaults{ true, 3.0, 3.5, "3", nullptr, 0, 11,
            std::numeric_limits<std::uint64_t>::max() };
        for (const auto& value : invalidDefaults) {
            auto document = schema;
            document["groups"][0]["settings"][1]["default"] = value;
            Reject(document, "default must be an integer within its bounds");
        }
        auto document = schema;
        document["groups"][0]["settings"][1].erase("default");
        Reject(document, "default must be an integer within its bounds");
        document = schema;
        document["groups"][0]["settings"][1]["min"] = 11;
        Reject(document, "min must not exceed max");
        for (const auto* bound : { "min", "max" }) {
            for (const auto& value : std::vector<Json>{ true, 1.0, "1", nullptr, std::numeric_limits<std::uint64_t>::max() }) {
                document = schema;
                document["groups"][0]["settings"][1][bound] = value;
                Reject(document, std::string(bound) + " must be a signed 64-bit integer");
            }
        }
        const std::int64_t exactValues[]{ std::numeric_limits<std::int64_t>::min(),
            -9007199254740993LL, 0, 9007199254740993LL, std::numeric_limits<std::int64_t>::max() };
        for (const auto value : exactValues) {
            document = schema;
            auto& setting = document["groups"][0]["settings"][1];
            setting.erase("min"); setting.erase("max"); setting["default"] = value;
            const auto unbounded = OSFSettings::SettingsJson::ParseSchema(Json::parse(document.dump()), error);
            Check(unbounded && unbounded->FindSetting("notificationLimit")->DefaultValue() == SettingValue{ value },
                "unbounded defaults preserve signed 64-bit integers, including values beyond double precision");
        }
        for (const auto* absent : { "min", "max" }) {
            document = schema;
            document["groups"][0]["settings"][1].erase(absent);
            Check(OSFSettings::SettingsJson::ParseSchema(document, error).has_value(), "each integer bound is optional");
        }
        document = schema;
        document["groups"][0]["settings"][1]["min"] = 3;
        document["groups"][0]["settings"][1]["max"] = 3;
        Check(OSFSettings::SettingsJson::ParseSchema(document, error).has_value(), "equal bounds allow their one valid integer");

        const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root = fs::current_path() / "build" / "tests" / "integers" / run;
        const auto schemas = root / "schemas";
        const auto values = root / "values";
        const auto valuesFile = values / "learning.json";
        const auto temporary = values / "learning.json.tmp";
        fs::create_directories(schemas);
        Write(schemas / "learning.json", schema.dump(2));
        OSFSettings::SettingsStore store;
        store.LoadAll(schemas, values);
        Check(store.LoadErrors().empty() && store.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{3} } &&
            store.GetValue("learning", "counter") == SettingValue{ std::int64_t{0} }, "integer defaults load without a saved file");
        Check(store.Set("learning", "notificationLimit", std::int64_t{3}).ok && !fs::exists(values),
            "setting the current integer does not write a file");
        Check(!store.Set("learning", "notificationLimit", false).ok &&
            !store.Set("learning", "notifications", std::int64_t{1}).ok && !fs::exists(values),
            "boolean and integer settings reject each other's value types without writing");

        OSFSettings::SettingsStore restarted;
        for (const std::int64_t value : { 1, 10, 7 }) {
            Check(store.Set("learning", "notificationLimit", value).ok, "integer edits accept both bounds and an interior value");
            restarted.LoadAll(schemas, values);
            Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationLimit") == SettingValue{ value },
                "accepted integer edits survive reloading");
        }
        const auto committed = Read(valuesFile);
        for (const auto value : { SettingValue{ true }, SettingValue{ std::int64_t{0} }, SettingValue{ std::int64_t{11} } }) {
            const auto result = store.Set("learning", "notificationLimit", value);
            Check(!result.ok && !result.error.empty() && Read(valuesFile) == committed &&
                store.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{7} },
                "invalid integer edits preserve both the live value and saved file");
        }
        fs::create_directory(temporary);
        Check(!store.Set("learning", "notificationLimit", std::int64_t{4}).ok && Read(valuesFile) == committed &&
            store.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{7} },
            "a failed integer save preserves both the live value and saved file");
        fs::remove(temporary); // Only the empty directory created by this test.

        for (const auto value : exactValues) {
            Check(store.Set("learning", "counter", value).ok, "the native store accepts the full signed 64-bit range");
            restarted.LoadAll(schemas, values);
            const auto saved = Json::parse(Read(valuesFile));
            Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "counter") == SettingValue{ value } &&
                saved["values"]["counter"].is_number_integer() && saved["values"]["counter"].get<std::int64_t>() == value,
                "JSON saving and loading preserve all integer bits");
        }
        Check(store.Set("learning", "notifications", false).ok, "booleans can still be saved in a mixed mod");
        restarted.LoadAll(schemas, values);
        Check(restarted.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{7} } &&
            restarted.GetValue("learning", "counter") == SettingValue{ std::numeric_limits<std::int64_t>::max() } &&
            restarted.GetValue("learning", "notifications") == SettingValue{ false }, "saving a boolean preserves neighboring integers");
        Check(store.Set("learning", "notificationLimit", limit->DefaultValue()).ok, "an integer can reset through the normal save path");
        restarted.LoadAll(schemas, values);
        Check(restarted.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{3} }, "the reset integer survives reload");

        for (const auto& value : invalidDefaults) {
            const Json saved = { { "formatVersion", 1 }, { "values", { { "notificationLimit", value }, { "notifications", false } } } };
            Write(valuesFile, saved.dump());
            restarted.LoadAll(schemas, values);
            Check(restarted.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{3} } &&
                restarted.GetValue("learning", "notifications") == SettingValue{ false } &&
                restarted.LoadErrors().size() == 1 && restarted.LoadErrors()[0].file == valuesFile &&
                restarted.LoadErrors()[0].message.find("notificationLimit") != std::string::npos && Read(valuesFile) == saved.dump(),
                "invalid saved integers retain defaults, report their key, and preserve valid neighbors and the file");
        }
        Write(valuesFile, Json{ { "formatVersion", 1 }, { "values", { { "notifications", false } } } }.dump());
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{3} },
            "an integer missing from saved values retains its default");
        std::cout << "Integer probe: default=3, bounds=1..10, exact signed 64-bit save/reload verified\n";
    }

    void TestStore(const Json& example, const fs::path& examplePath)
    {
        // Keep generated fixtures under build so the walkthrough can inspect them.
        const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root = fs::current_path() / "build" / "tests" / "checkpoint1" / run;
        const auto schemas = root / "schemas";
        const auto values = root / "values";
        fs::create_directories(schemas);

        OSFSettings::SettingsStore store;
        store.LoadAll(schemas, values);
        Check(store.Mods().empty() && store.LoadErrors().empty(), "an empty directory is valid");

        Write(schemas / "learning.json", example.dump(2));
        Write(schemas / "broken.json", "{ malformed json");
        Write(schemas / "ignored.txt", "not a schema");
        auto document = example;
        document["id"] = "unsupported";
        document["schemaVersion"] = 2;
        Write(schemas / "unsupported.json", document.dump());
        Write(schemas / "mismatch.json", example.dump());

        store.LoadAll(schemas, values);
        Check(store.Mods().size() == 1 && store.LoadErrors().size() == 3,
            "valid schemas survive malformed, unsupported, and mismatched neighboring files");
        Check(store.GetValue("learning", "notifications") == SettingValue{ true }, "the store owns the true default");
        Check(!store.GetValue("missing", "notifications").has_value(), "unknown mod returns no value");
        Check(!store.GetValue("learning", "missing").has_value(), "unknown key returns no value");
        Check(!store.GetValue("learning", "Notifications").has_value(), "setting keys are case-sensitive");

        auto ownedCopy = store.GetValue("learning", "notifications");
        ownedCopy = false;
        Check(ownedCopy == SettingValue{ false } && store.GetValue("learning", "notifications") == SettingValue{ true },
            "changing a returned copy does not edit the store");

        document = example;
        document["groups"][0]["settings"][0]["default"] = false;
        Write(schemas / "learning.json", document.dump());
        store.LoadAll(schemas, values);
        Check(store.Mods().size() == 1 && store.GetValue("learning", "notifications") == SettingValue{ false },
            "reloading replaces defaults without duplicating mods; false is not a missing value");

        store.LoadAll(root / "missing", values);
        Check(store.Mods().empty() && store.LoadErrors().size() == 1,
            "a missing directory reports an error and clears stale values");
        store.LoadAll(schemas / "learning.json", values);
        Check(store.Mods().empty() && store.LoadErrors().size() == 1, "a file is not accepted as the schema directory");

        store.LoadAll(examplePath.parent_path(), values);
        Check(store.LoadErrors().empty() && store.GetValue("learning", "notifications") == SettingValue{ true },
            "the actual shipped schema loads through the production store");
        if (const auto value = store.GetValue("learning", "notifications")) {
            std::cout << "Schema probe: learning / notifications = " << std::boolalpha << std::get<bool>(*value) << " (schema default)\n";
        }
    }

    void TestPersistence(const Json& example)
    {
        const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root = fs::current_path() / "build" / "tests" / "checkpoint2" / run;
        const auto schemas = root / "schemas";
        const auto values = root / "values";
        const auto valuesFile = values / "learning.json";
        const auto temporary = values / "learning.json.tmp";
        fs::create_directories(schemas);

        auto schema = example;
        schema["groups"][0]["settings"].push_back({ { "key", "quiet" }, { "type", "bool" }, { "default", false } });
        Write(schemas / "learning.json", schema.dump(2));
        const auto originalSchema = Read(schemas / "learning.json");
        schema["id"] = "other";
        Write(schemas / "other.json", schema.dump(2));

        OSFSettings::SettingsStore store;
        store.LoadAll(schemas, values);
        Check(store.LoadErrors().empty() && store.GetValue("learning", "notifications") == SettingValue{ true },
            "missing saved values use schema defaults without an error");
        Check(!fs::exists(values), "loading does not create values files or directories");
        const auto missingMod = store.Set("missing", "notifications", false);
        const auto missingKey = store.Set("learning", "missing", false);
        Check(!missingMod.ok && !missingMod.error.empty() && !missingKey.ok && !missingKey.error.empty(),
            "setting an unknown mod or key reports an error");
        Check(!fs::exists(values), "rejected edits do not write any files");
        Check(store.Set("learning", "notifications", true).ok && !fs::exists(values),
            "setting the current value succeeds without a disk write");

        const auto disabled = store.Set("learning", "notifications", false);
        Check(disabled.ok && disabled.error.empty() && store.GetValue("learning", "notifications") == SettingValue{ false },
            "a successful save publishes the new boolean");
        const auto saved = Json::parse(Read(valuesFile));
        Check(saved["formatVersion"] == 1 && saved["values"]["notifications"] == false && saved["values"]["quiet"] == false,
            "the values file contains the version and all current booleans for this mod");
        Check(!fs::exists(temporary), "successful replacement leaves no temporary file");
        Check(store.GetValue("other", "notifications") == SettingValue{ true } && !fs::exists(values / "other.json"),
            "saving one mod does not change another mod");
        Check(Read(schemas / "learning.json") == originalSchema && std::get<bool>(store.Mods()[0].schema.FindSetting("notifications")->DefaultValue()),
            "saving changes neither the authored schema nor its in-memory default");

        OSFSettings::SettingsStore restarted;
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notifications") == SettingValue{ false },
            "a fresh store reads the saved false override");
        std::cout << "Persistence probe: default=true, saved=" << std::boolalpha << saved["values"]["notifications"].get<bool>()
                  << ", reloaded=" << std::get<bool>(restarted.GetValue("learning", "notifications").value()) << '\n';

        Check(store.Set("learning", "quiet", true).ok && store.Set("other", "notifications", false).ok,
            "other settings and mods can be saved independently");
        restarted.LoadAll(schemas, values);
        Check(restarted.GetValue("learning", "notifications") == SettingValue{ false } && restarted.GetValue("learning", "quiet") == SettingValue{ true } &&
            restarted.GetValue("other", "notifications") == SettingValue{ false } && restarted.GetValue("other", "quiet") == SettingValue{ false },
            "saving another key preserves its neighbor and keeps mod values separate");
        Check(store.Set("learning", "notifications", true).ok, "a value can be changed back to true");
        restarted.LoadAll(schemas, values);
        Check(restarted.GetValue("learning", "notifications") == SettingValue{ true }, "a saved true value also survives reload");

        // A leftover temporary file from an interrupted write is never loaded.
        Write(temporary, "incomplete write");
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notifications") == SettingValue{ true },
            "reload uses the committed file and ignores a leftover temporary file");
        Check(store.Set("learning", "notifications", false).ok && !fs::exists(temporary),
            "the next successful edit replaces a stale temporary file");

        const auto committed = Read(valuesFile);
        fs::create_directory(temporary); // Force failure before the temporary file can be opened.
        const auto failedOpen = store.Set("learning", "notifications", true);
        Check(!failedOpen.ok && !failedOpen.error.empty() && store.GetValue("learning", "notifications") == SettingValue{ false },
            "failure to open the temporary file rejects the edit");
        Check(Read(valuesFile) == committed && fs::is_directory(temporary), "failed open preserves the saved file and the pre-existing blocker");
        fs::remove(temporary); // Only the empty directory created by this test.

#ifdef _WIN32
        // Denying delete sharing makes the actual Windows replacement fail.
        const auto locked = ::CreateFileW(valuesFile.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(locked != INVALID_HANDLE_VALUE, "the replacement-failure fixture can lock the saved file");
        if (locked != INVALID_HANDLE_VALUE) {
            const auto failedReplace = store.Set("learning", "notifications", true);
            ::CloseHandle(locked);
            Check(!failedReplace.ok && !failedReplace.error.empty() && store.GetValue("learning", "notifications") == SettingValue{ false },
                "failed atomic replacement leaves the live value unchanged");
            Check(Read(valuesFile) == committed && !fs::exists(temporary),
                "failed atomic replacement preserves the old file and cleans up its temporary file");
            Check(store.Set("learning", "notifications", true).ok, "saving can be retried after the replacement failure is removed");
        }
#endif

        const auto blocked = root / "blocked";
        Write(blocked, "this is a file, not a directory");
        restarted.LoadAll(schemas, blocked / "values");
        const auto failedDirectory = restarted.Set("learning", "notifications", false);
        Check(!failedDirectory.ok && !failedDirectory.error.empty() && restarted.GetValue("learning", "notifications") == SettingValue{ true },
            "failure to create the values directory preserves the live default");

        const Json mixed = { { "formatVersion", 1 }, { "values", { { "notifications", "false" }, { "quiet", true }, { "removed", 123 } } } };
        Write(valuesFile, mixed.dump());
        restarted.LoadAll(schemas, values);
        Check(restarted.GetValue("learning", "notifications") == SettingValue{ true } && restarted.GetValue("learning", "quiet") == SettingValue{ true } &&
            !restarted.GetValue("learning", "removed").has_value(), "only known, correctly typed saved values override defaults");
        Check(restarted.LoadErrors().size() == 1 && restarted.LoadErrors()[0].file == valuesFile &&
            restarted.LoadErrors()[0].message.find("notifications") != std::string::npos,
            "a wrong-type saved boolean reports its path and key");
        Write(valuesFile, Json{ { "formatVersion", 1 }, { "values", { { "quiet", true } } } }.dump());
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notifications") == SettingValue{ true } &&
            restarted.GetValue("learning", "quiet") == SettingValue{ true }, "settings absent from a saved file keep their defaults");

        const std::vector<Json> invalidFiles{
            Json::array(), Json::object(),
            { { "formatVersion", 2 }, { "values", { { "notifications", false } } } },
            { { "formatVersion", 1.0 }, { "values", Json::object() } },
            { { "formatVersion", "1" }, { "values", Json::object() } },
            { { "formatVersion", true }, { "values", Json::object() } },
            { { "formatVersion", 1 }, { "values", Json::array() } },
            { { "formatVersion", 1 } }
        };
        for (const auto& invalid : invalidFiles) {
            Write(valuesFile, invalid.dump());
            restarted.LoadAll(schemas, values);
            Check(restarted.GetValue("learning", "notifications") == SettingValue{ true } && restarted.LoadErrors().size() == 1 &&
                restarted.GetValue("other", "notifications") == SettingValue{ false } && Read(valuesFile) == invalid.dump(),
                "invalid values documents retain defaults and other mods without rewriting the file");
        }

        Write(valuesFile, "{ malformed json");
        restarted.LoadAll(schemas, values);
        Check(restarted.GetValue("learning", "notifications") == SettingValue{ true } && restarted.LoadErrors().size() == 1 && Read(valuesFile) == "{ malformed json",
            "malformed saved JSON is reported and preserved on load");
        Check(restarted.Set("learning", "notifications", false).ok, "an explicit valid edit can replace a malformed saved file");
        OSFSettings::SettingsStore recovered;
        recovered.LoadAll(schemas, values);
        Check(recovered.LoadErrors().empty() && recovered.GetValue("learning", "notifications") == SettingValue{ false },
            "the recovered file reloads without errors");
    }
}

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path examplePath = argc > 1 ? argv[1] :
            "data/SFSE/Plugins/OSF/Settings/schemas/learning.json";
        std::ifstream input(examplePath);
        if (!input) throw std::runtime_error("cannot open example schema: " + examplePath.string());
        const auto example = nlohmann::json::parse(input);
        TestSchema(example);
        TestStore(example, examplePath);
        TestPersistence(example);
        TestIntegers(example);
        std::cout << checks - failures << '/' << checks << " checks passed\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Test setup failed: " << error.what() << '\n';
        return 1;
    }
}
