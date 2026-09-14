#include "Settings/SettingsJson.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace
{
    namespace fs = std::filesystem;
    using Json = nlohmann::json;
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

    void TestSchema(const Json& example)
    {
        std::string error = "old error";
        const auto schema = OSFSettings::SettingsJson::ParseSchema(example, error);
        Check(schema.has_value() && error.empty(), "the shipped example parses and clears the error");
        if (!schema) return;
        Check(schema->id == "learning" && schema->groups.size() == 1, "mod and group are loaded");
        const auto* setting = schema->FindSetting("notifications");
        Check(setting && setting->defaultValue && setting->label == "Enable notifications", "boolean default and label are loaded");
        Check(schema->FindSetting("unknown") == nullptr, "unknown definition is absent");

        auto document = example;
        document["groups"][0]["settings"][0]["default"] = false;
        auto parsed = OSFSettings::SettingsJson::ParseSchema(document, error);
        Check(parsed && !parsed->groups[0].settings[0].defaultValue, "false is a valid default");

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

        for (const auto* type : { "int", "float", "enum", "string", "key", "flags", "action", "note" }) {
            document = example;
            document["groups"][0]["settings"][0]["type"] = type;
            Reject(document, "only type bool");
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

    void TestStore(const Json& example, const fs::path& examplePath)
    {
        // Keep generated fixtures under build so the walkthrough can inspect them.
        const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root = fs::current_path() / "build" / "tests" / "checkpoint1" / run;
        const auto schemas = root / "schemas";
        fs::create_directories(schemas);

        OSFSettings::SettingsStore store;
        store.LoadAll(schemas);
        Check(store.Mods().empty() && store.LoadErrors().empty(), "an empty directory is valid");

        Write(schemas / "learning.json", example.dump(2));
        Write(schemas / "broken.json", "{ malformed json");
        Write(schemas / "ignored.txt", "not a schema");
        auto document = example;
        document["id"] = "unsupported";
        document["schemaVersion"] = 2;
        Write(schemas / "unsupported.json", document.dump());
        Write(schemas / "mismatch.json", example.dump());

        store.LoadAll(schemas);
        Check(store.Mods().size() == 1 && store.LoadErrors().size() == 3,
            "valid schemas survive malformed, unsupported, and mismatched neighboring files");
        Check(store.GetValue("learning", "notifications") == std::optional<bool>(true), "the store owns the true default");
        Check(!store.GetValue("missing", "notifications").has_value(), "unknown mod returns no value");
        Check(!store.GetValue("learning", "missing").has_value(), "unknown key returns no value");
        Check(!store.GetValue("learning", "Notifications").has_value(), "setting keys are case-sensitive");

        auto ownedCopy = store.GetValue("learning", "notifications");
        ownedCopy = false;
        Check(ownedCopy == false && store.GetValue("learning", "notifications") == true,
            "changing a returned copy does not edit the store");

        document = example;
        document["groups"][0]["settings"][0]["default"] = false;
        Write(schemas / "learning.json", document.dump());
        store.LoadAll(schemas);
        Check(store.Mods().size() == 1 && store.GetValue("learning", "notifications") == false,
            "reloading replaces defaults without duplicating mods; false is not a missing value");

        store.LoadAll(root / "missing");
        Check(store.Mods().empty() && store.LoadErrors().size() == 1,
            "a missing directory reports an error and clears stale values");
        store.LoadAll(schemas / "learning.json");
        Check(store.Mods().empty() && store.LoadErrors().size() == 1, "a file is not accepted as the schema directory");

        store.LoadAll(examplePath.parent_path());
        Check(store.LoadErrors().empty() && store.GetValue("learning", "notifications") == true,
            "the actual shipped schema loads through the production store");
        if (const auto value = store.GetValue("learning", "notifications")) {
            std::cout << "Schema probe: learning / notifications = " << std::boolalpha << *value << " (schema default)\n";
        }
    }
}

int main(int argc, char** argv)
{
    try {
        const std::filesystem::path examplePath = argc > 1 ? argv[1] :
            "data/SFSE/Plugins/OSF/SettingsSlim/schemas/learning.json";
        std::ifstream input(examplePath);
        if (!input) throw std::runtime_error("cannot open example schema: " + examplePath.string());
        const auto example = nlohmann::json::parse(input);
        TestSchema(example);
        TestStore(example, examplePath);
        std::cout << checks - failures << '/' << checks << " checks passed\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Test setup failed: " << error.what() << '\n';
        return 1;
    }
}
