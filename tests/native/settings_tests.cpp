#include "Settings/SettingsJson.h"
#include "Menu/FloatSlider.h"
#include "Settings/SettingsStore.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

// This suite tests persistence, not the game's keyboard-name table. The authored
// learning fixture has one F4 key default; fail on unexpected engine lookups.
namespace OSFSettings
{
    std::optional<std::uint32_t> KeyCodeFromName(std::string_view name)
    {
        if (name == "F4") return 0x73;
        throw std::runtime_error("unexpected key-name lookup in storage fixture");
    }
    bool IsBindableKey(std::uint32_t code) { return code == 0x73; }
}

namespace
{
    namespace fs = std::filesystem;
    using Json = nlohmann::ordered_json;
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
        const auto schema = OSFSettings::SettingsJson::ParseSchema(document, "learning", error);
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

    void TestIntegers(const Json& example)
    {
        auto schema = example;

        schema["groups"]["General"].push_back({ { "key", "counter" }, { "type", "int" }, { "default", 0 } });
        std::string error;
        const auto parsed = OSFSettings::SettingsJson::ParseSchema(schema, "learning", error);
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
            document["groups"]["General"][1]["default"] = value;
            Reject(document, "default must be an integer within its bounds");
        }
        auto document = schema;
        document["groups"]["General"][1].erase("default");
        Reject(document, "default must be an integer within its bounds");
        document = schema;
        document["groups"]["General"][1]["min"] = 11;
        Reject(document, "min must not exceed max");
        for (const auto* bound : { "min", "max" }) {
            for (const auto& value : std::vector<Json>{ true, 1.0, "1", nullptr, std::numeric_limits<std::uint64_t>::max() }) {
                document = schema;
                document["groups"]["General"][1][bound] = value;
                Reject(document, std::string(bound) + " must be a signed 64-bit integer");
            }
        }
        const std::int64_t exactValues[]{ std::numeric_limits<std::int64_t>::min(),
            -9007199254740993LL, 0, 9007199254740993LL, std::numeric_limits<std::int64_t>::max() };
        for (const auto value : exactValues) {
            document = schema;
            auto& setting = document["groups"]["General"][1];
            setting.erase("min"); setting.erase("max"); setting["default"] = value;
            const auto unbounded = OSFSettings::SettingsJson::ParseSchema(Json::parse(document.dump()), "learning", error);
            Check(unbounded && unbounded->FindSetting("notificationLimit")->DefaultValue() == SettingValue{ value },
                "unbounded defaults preserve signed 64-bit integers, including values beyond double precision");
        }
        for (const auto* absent : { "min", "max" }) {
            document = schema;
            document["groups"]["General"][1].erase(absent);
            Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "each integer bound is optional");
        }
        document = schema;
        document["groups"]["General"][1]["min"] = 3;
        document["groups"]["General"][1]["max"] = 3;
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "equal bounds allow their one valid integer");

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
                restarted.LoadErrors().size() == 1 && restarted.LoadErrors()[0].file == valuesFile && !restarted.LoadErrors()[0].schema &&
                restarted.LoadErrors()[0].message.find("notificationLimit") != std::string::npos && Read(valuesFile) == saved.dump(),
                "invalid saved integers retain defaults, report their key as a values error, and preserve valid neighbors and the file");
        }
        Write(valuesFile, Json{ { "formatVersion", 1 }, { "values", { { "notifications", false } } } }.dump());
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{3} },
            "an integer missing from saved values retains its default");
        std::cout << "Integer probe: default=3, bounds=1..10, exact signed 64-bit save/reload verified\n";
    }

    void TestFloatSlider(const Json& example)
    {
        std::string error;
        const auto schema = OSFSettings::SettingsJson::ParseSchema(example, "learning", error);
        const auto* setting = schema ? schema->FindSetting("notificationVolume") : nullptr;
        const auto* definition = setting ? std::get_if<OSFSettings::FloatDefinition>(&setting->definition) : nullptr;
        Check(definition && definition->defaultValue == 0.75 && definition->step == 0.05, "the shipped float has its declared default and step");
        if (!definition) return;
        const auto slider = OSFSettings::MakeFloatSlider(*definition);
        Check(slider && slider->minimum == 0 && slider->maximum == 100 && slider->step == 5 &&
            slider->scale == 100 && slider->steps == 20 && slider->decimals == 2, "volume uses twenty exact 0.05 increments and two decimal places");
        if (!slider) return;
        for (std::uint32_t position = 0; position <= slider->steps; ++position) {
            const double value = static_cast<double>(slider->minimum + position * slider->step) / static_cast<double>(slider->scale);
            Check(OSFSettings::IsValidValue(*setting, SettingValue{value}) && value == static_cast<double>(position) / 20.0,
                "every sample slider position is a valid canonical decimal, including both bounds");
        }
        const auto make = [](double minimum, double maximum, double step) {
            OSFSettings::FloatDefinition value;
            value.minimum = minimum; value.maximum = maximum; value.step = step;
            return OSFSettings::MakeFloatSlider(value);
        };
        const auto negative = make(-0.15, 0.35, 0.1);
        Check(negative && negative->minimum == -15 && negative->maximum == 35 && negative->step == 10 && negative->steps == 5,
            "negative decimal offsets are preserved even when more precise than the step");
        const auto partial = make(0.0, 1.0, 0.3);
        Check(partial && partial->steps == 4 && partial->minimum + (partial->steps - 1) * partial->step == 9 && partial->maximum == 10,
            "a non-divisible range ends with a shorter final step to its exact maximum");
        Check(make(0.0, 0.07, 0.01)->steps == 7, "binary division noise does not add an eighth step");
        Check(make(0.0, 1.0, 2.0)->steps == 1, "a step larger than the range still allows both endpoints");
        Check(make(0.0, 0.000000005, 0.000000001)->steps == 5, "nine-place decimal steps are supported");
        Check(make(0.0, 4294967295.0, 1.0)->steps == std::numeric_limits<std::uint32_t>::max(), "the vanilla uint32 step limit is supported");
        Check(!make(0.0, 4294967296.0, 1.0), "ranges exceeding the vanilla step count remain read-only");
        Check(!make(0.0, 1.0, 0.0000000001), "finer than nine-place decimal steps remain read-only");
        Check(!make(-9007199254740991.0, 9007199254740991.0, 9007199254740991.0), "scaled spans must also fit AS3's exact integer range");
        Check(!make(900719925474098.0, 900719925474099.0, 0.1), "steps smaller than double precision at the bounds remain read-only");
        Check(!make(1.0, 1.0, 0.1) && !make(2.0, 1.0, 0.1), "fixed and reversed ranges do not create sliders");
        Check(!make(0.0, 1.0, 0.0) && !make(0.0, 1.0, -1.0) && !make(0.0, 1.0, std::numeric_limits<double>::infinity()),
            "invalid increments do not create sliders");
        Check(!OSFSettings::MakeFloatSlider(OSFSettings::FloatDefinition{}), "unbounded floats do not create sliders");
        Check(OSFSettings::IsValidValue(*setting, SettingValue{0.733}), "editor increments do not reject an existing off-step value");
    }

    void TestFloats(const Json& example)
    {
        auto schema = example;
        const auto gainIndex = schema["groups"]["General"].size();
        schema["groups"]["General"].push_back({ { "key", "gain" }, { "type", "float" },
            { "default", 0.75 }, { "min", 0 }, { "max", 1.0 } });
        schema["groups"]["General"].push_back({ { "key", "scale" }, { "type", "float" }, { "default", 0.0 } });
        std::string error;
        const auto parsed = OSFSettings::SettingsJson::ParseSchema(schema, "learning", error);
        Check(parsed.has_value() && error.empty(), "a schema can mix booleans, integers, and floats");
        if (!parsed) return;
        const auto* gain = parsed->FindSetting("gain");
        const auto* floating = gain ? std::get_if<OSFSettings::FloatDefinition>(&gain->definition) : nullptr;
        Check(floating && floating->defaultValue == 0.75 && floating->minimum == 0.0 && floating->maximum == 1.0 && floating->step == 0.1,
            "float definition, decimal default, and numeric inclusive bounds are loaded");
        if (!gain) return;

        const auto nan = std::numeric_limits<double>::quiet_NaN();
        const auto infinity = std::numeric_limits<double>::infinity();
        for (const auto& step : std::vector<Json>{ 0, -0.1, true, "0.1", nullptr, nan, infinity, -infinity }) {
            auto document = schema;
            document["groups"]["General"][gainIndex]["step"] = step;
            Reject(document, "step must");
        }
        const std::vector<Json> invalidDefaults{ true, "0.5", nullptr, -0.01, 1.01, Json::array(), Json::object(), nan, infinity, -infinity };
        for (const auto& value : invalidDefaults) {
            auto document = schema;
            document["groups"]["General"][gainIndex]["default"] = value;
            Reject(document, "default must be a finite number within its bounds");
        }
        auto document = schema;
        document["groups"]["General"][gainIndex].erase("default");
        Reject(document, "default must be a finite number within its bounds");
        document = schema;
        document["groups"]["General"][gainIndex]["min"] = 2.0;
        Reject(document, "min must not exceed max");
        for (const auto* bound : { "min", "max" }) {
            for (const auto& value : std::vector<Json>{ true, "1", nullptr, nan, infinity, -infinity }) {
                document = schema;
                document["groups"]["General"][gainIndex][bound] = value;
                Reject(document, std::string(bound) + " must be a finite number");
            }
            document = schema;
            document["groups"]["General"][gainIndex].erase(bound);
            Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "each float bound is optional");
        }
        document = schema;
        auto& fixed = document["groups"]["General"][gainIndex];
        fixed["min"] = 0.75; fixed["max"] = 0.75;
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "equal bounds allow their one valid float");
        document = schema;
        auto& negative = document["groups"]["General"][gainIndex];
        negative["default"] = -0.75; negative["min"] = -1; negative["max"] = -0.5;
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "float ranges and defaults can be negative");
        for (const auto* literal : { "0", "1", "1.0", "1e0", "0.1" }) {
            document = schema;
            const auto value = Json::parse(literal);
            document["groups"]["General"][gainIndex]["default"] = value;
            const auto decoded = OSFSettings::SettingsJson::ParseSchema(document, "learning", error);
            Check(decoded && decoded->FindSetting("gain")->DefaultValue() == SettingValue{ value.get<double>() },
                "integer, decimal, and exponent JSON defaults become doubles for float definitions");
        }
        document = schema;
        document["groups"]["General"][gainIndex + 1]["default"] = std::numeric_limits<std::uint64_t>::max();
        const auto wide = OSFSettings::SettingsJson::ParseSchema(document, "learning", error);
        Check(wide && wide->FindSetting("scale")->DefaultValue() == SettingValue{ static_cast<double>(std::numeric_limits<std::uint64_t>::max()) },
            "float JSON decoding is not limited by signed integer storage");

        const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root = fs::current_path() / "build" / "tests" / "floats" / run;
        const auto schemas = root / "schemas";
        const auto values = root / "values";
        const auto valuesFile = values / "learning.json";
        const auto temporary = values / "learning.json.tmp";
        fs::create_directories(schemas);
        Write(schemas / "learning.json", schema.dump(2));
        OSFSettings::SettingsStore store;
        store.LoadAll(schemas, values);
        Check(store.LoadErrors().empty() && store.GetValue("learning", "gain") == SettingValue{0.75} &&
            store.GetValue("learning", "scale") == SettingValue{0.0}, "bounded and unbounded float defaults load without a saved file");
        Check(store.Set("learning", "gain", 0.75).ok && !fs::exists(values), "setting the current float does not write a file");
        Check(!store.Set("learning", "gain", true).ok && !store.Set("learning", "gain", std::int64_t{1}).ok &&
            !store.Set("learning", "notifications", 1.0).ok && !store.Set("learning", "notificationLimit", 3.0).ok && !fs::exists(values),
            "native edits require the declared variant type, even for integral doubles");

        OSFSettings::SettingsStore restarted;
        for (const double value : { 0.0, 1.0, 0.1, std::nextafter(0.1, 1.0), 0.625 }) {
            Check(store.Set("learning", "gain", value).ok, "float edits accept both bounds and interior decimal values");
            restarted.LoadAll(schemas, values);
            Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "gain") == SettingValue{value},
                "accepted float edits survive reload without rounding to a nearby double");
        }
        const auto committed = Read(valuesFile);
        for (const auto value : { SettingValue{true}, SettingValue{std::int64_t{1}}, SettingValue{std::nextafter(0.0, -1.0)},
            SettingValue{std::nextafter(1.0, 2.0)}, SettingValue{nan}, SettingValue{infinity}, SettingValue{-infinity} }) {
            const auto result = store.Set("learning", "gain", value);
            Check(!result.ok && !result.error.empty() && Read(valuesFile) == committed &&
                store.GetValue("learning", "gain") == SettingValue{0.625}, "invalid float edits preserve the live value and saved file");
        }
        for (const double value : { nan, infinity, -infinity }) {
            Check(!store.Set("learning", "scale", value).ok, "unbounded float settings also reject non-finite edits");
            Check(!OSFSettings::SettingsJson::SaveValues(valuesFile, { { "gain", value } }, error) &&
                error.find("value must be finite: gain") != std::string::npos && Read(valuesFile) == committed && !fs::exists(temporary),
                "serialization rejects non-finite doubles before JSON can replace them with null");
        }
        fs::create_directory(temporary);
        Check(!store.Set("learning", "gain", 0.25).ok && Read(valuesFile) == committed &&
            store.GetValue("learning", "gain") == SettingValue{0.625}, "a failed float save preserves the live value and saved file");
        fs::remove(temporary); // Only the empty directory created by this test.

        for (const double value : { -std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
            std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min(), -std::numeric_limits<double>::denorm_min(),
            std::nextafter(1.0, 2.0), -0.125, 0.0, 0.1 }) {
            Check(store.Set("learning", "scale", value).ok, "unbounded float edits accept finite extremes and small fractions");
            restarted.LoadAll(schemas, values);
            const auto saved = Json::parse(Read(valuesFile));
            Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "scale") == SettingValue{value} &&
                saved["values"]["scale"].is_number_float() && saved["values"]["scale"].get<double>() == value,
                "finite doubles round trip as JSON numbers, including subnormal values");
        }
        Check(store.Set("learning", "notifications", false).ok && store.Set("learning", "notificationLimit", std::int64_t{7}).ok,
            "booleans and integers can still be edited beside floats");
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notifications") == SettingValue{false} &&
            restarted.GetValue("learning", "notificationLimit") == SettingValue{std::int64_t{7}} &&
            restarted.GetValue("learning", "gain") == SettingValue{0.625} && restarted.GetValue("learning", "scale") == SettingValue{0.1},
            "saving boolean and integer neighbors preserves decimal values");
        Check(store.Set("learning", "gain", gain->DefaultValue()).ok, "floats reset through the normal save path");
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "gain") == SettingValue{0.75}, "reset float defaults survive reload");

        for (const auto* literal : { "0", "1", "1.0", "1e0", "0.1" }) {
            const auto value = Json::parse(literal);
            const Json saved = { { "formatVersion", 1 }, { "values", { { "gain", value } } } };
            Write(valuesFile, saved.dump());
            restarted.LoadAll(schemas, values);
            Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "gain") == SettingValue{value.get<double>()},
                "saved JSON integer and decimal literals load as doubles for float settings");
        }
        for (const auto& value : std::vector<Json>{ true, "0.5", nullptr, -0.01, 1.01, Json::array(), Json::object() }) {
            const Json saved = { { "formatVersion", 1 }, { "values", { { "gain", value }, { "notifications", false }, { "notificationLimit", 7 } } } };
            Write(valuesFile, saved.dump());
            restarted.LoadAll(schemas, values);
            Check(restarted.GetValue("learning", "gain") == SettingValue{0.75} && restarted.GetValue("learning", "notifications") == SettingValue{false} &&
                restarted.GetValue("learning", "notificationLimit") == SettingValue{std::int64_t{7}} && restarted.LoadErrors().size() == 1 &&
                restarted.LoadErrors()[0].file == valuesFile && restarted.LoadErrors()[0].message.find("gain") != std::string::npos && Read(valuesFile) == saved.dump(),
                "invalid saved floats retain defaults, report their key, and preserve valid neighbors and the file");
        }
        for (const auto* literal : { "NaN", "Infinity", "1e400" }) {
            const auto saved = std::string("{\"formatVersion\":1,\"values\":{\"gain\":") + literal + "}}";
            Write(valuesFile, "{\"formatVersion\":1,\"settings\":{\"learning\":" + saved + "}}");
            restarted.LoadAll(schemas, values);
            Check(restarted.GetValue("learning", "gain") == SettingValue{0.75} && restarted.LoadErrors().size() == 1 && Read(valuesFile) == "{\"formatVersion\":1,\"settings\":{\"learning\":" + saved + "}}",
                "malformed or overflowing JSON numbers are rejected without rewriting the file");
        }
        Write(valuesFile, Json{ { "formatVersion", 1 }, { "values", { { "notifications", false } } } }.dump());
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "gain") == SettingValue{0.75}, "an absent saved float retains its default");
        std::cout << "Float probe: default=0.75, bounds=0..1, finite validation and decimal save/reload verified\n";
    }

    void TestEnums(const Json& example)
    {
        auto schema = example;
        const auto& settings = schema["groups"]["General"];
        const auto modeEntry = std::ranges::find_if(settings, [](const Json& setting) { return setting["key"] == "notificationMode"; });
        const auto modeIndex = static_cast<std::size_t>(std::distance(settings.begin(), modeEntry));
        std::string error;
        const auto parsed = OSFSettings::SettingsJson::ParseSchema(schema, "learning", error);
        Check(parsed.has_value() && error.empty(), "a schema can mix enums with booleans, integers, and floats");
        if (!parsed) return;
        const auto* mode = parsed->FindSetting("notificationMode");
        const auto* enumeration = mode ? std::get_if<OSFSettings::EnumDefinition>(&mode->definition) : nullptr;
        Check(enumeration && enumeration->defaultValue.value == "normal" && mode->DefaultValue() == SettingValue{ OSFSettings::EnumValue{"normal"} },
            "enum defaults retain typed option identities");
        Check(enumeration && enumeration->options.size() == 3 &&
            enumeration->options[0].value == "quiet" && enumeration->options[0].label == "Quiet" &&
            enumeration->options[1].value == "normal" && enumeration->options[1].label == "Normal" &&
            enumeration->options[2].value == "verbose" && enumeration->options[2].label == "Verbose",
            "enum options preserve authored order and pair each value with its label");
        if (!enumeration) return;

        auto document = schema;
        document["groups"]["General"][modeIndex]["options"] = { "quiet", "normal", "verbose" };
        auto decoded = OSFSettings::SettingsJson::ParseSchema(document, "learning", error);
        Check(decoded && std::get<OSFSettings::EnumDefinition>(decoded->FindSetting("notificationMode")->definition).options[0].value == "quiet" &&
            std::get<OSFSettings::EnumDefinition>(decoded->FindSetting("notificationMode")->definition).options[0].label == "quiet",
            "string array options use each string as both value and label");
        document = schema;
        document["groups"]["General"][modeIndex]["options"] = { { "quiet", "" }, { "normal", "Same label" }, { "verbose", "Same label" } };
        decoded = OSFSettings::SettingsJson::ParseSchema(document, "learning", error);
        Check(decoded && std::get<OSFSettings::EnumDefinition>(decoded->FindSetting("notificationMode")->definition).options[0].label == "quiet",
            "empty labels use the option value and display labels need not be unique");
        document = schema;
        auto& single = document["groups"]["General"][modeIndex];
        single["options"] = { { "normal", "Normal" } };
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "an enum object can have a single option");
        single["options"] = { "normal" };
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "an enum array can have a single option");
        single["options"] = { "normal", "Normal" };
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "enum option identities are case-sensitive");
        single["options"] = { { "normal", "Normal" }, { "Normal", "Normal" } };
        Check(OSFSettings::SettingsJson::ParseSchema(document, "learning", error).has_value(), "enum object keys are case-sensitive");

        std::istringstream duplicateOptions(R"({"groups":{"General":[
            {"key":"mode","type":"enum","default":"normal","options":{"normal":"Normal","normal":"Repeated"}}
        ]}})");
        Check(!OSFSettings::SettingsJson::ParseSchema(duplicateOptions, "learning", error) && error == "duplicate option: normal",
            "duplicate source option values cannot silently overwrite labels");
        std::istringstream sharedOptions(R"({"groups":{"General":[
            {"key":"first","type":"enum","default":"normal","options":{"verbose":"Verbose","normal":"Normal"}},
            {"key":"second","type":"enum","default":"normal","options":{"normal":"Normal"}}
        ]}})");
        decoded = OSFSettings::SettingsJson::ParseSchema(sharedOptions, "learning", error);
        Check(decoded && std::get<OSFSettings::EnumDefinition>(decoded->FindSetting("first")->definition).options[0].value == "verbose",
            "source objects preserve authored order and separate settings may reuse option values");

        document = schema;
        document["groups"]["General"][modeIndex].erase("options");
        Reject(document, "options must be a non-empty array or object");
        for (const auto& options : std::vector<Json>{ Json::array(), Json::object(), nullptr, true, 1, "quiet" }) {
            document = schema;
            document["groups"]["General"][modeIndex]["options"] = options;
            Reject(document, "options must be a non-empty array or object");
        }
        for (const auto& option : std::vector<Json>{ "", true, 1, 1.0, nullptr, Json::array(), Json::object() }) {
            document = schema;
            document["groups"]["General"][modeIndex]["options"] = Json::array({ option, "normal" });
            Reject(document, "each option must be a non-empty string");
        }
        document = schema;
        document["groups"]["General"][modeIndex]["options"] = { "normal", "normal" };
        Reject(document, "duplicate option");
        for (const auto& value : { std::string{}, std::string("bad\0option", 10) }) {
            document = schema;
            document["groups"]["General"][modeIndex]["options"][value] = "Invalid";
            Reject(document, value.empty() ? "each option must be a non-empty string" : "option value must not contain NUL");
        }
        document = schema;
        document["groups"]["General"][modeIndex]["options"] = { "normal", std::string("bad\0option", 10) };
        Reject(document, "option value must not contain NUL");
        document = schema;
        document["groups"]["General"][modeIndex]["optionLabels"] = { "Quiet", "Normal", "Verbose" };
        Reject(document, "optionLabels is no longer supported; use an options object");
        for (const auto& label : std::vector<Json>{ nullptr, true, 1, Json::array(), Json::object() }) {
            document = schema;
            document["groups"]["General"][modeIndex]["options"]["quiet"] = label;
            Reject(document, "each option label must be a string");
        }
        document = schema;
        document["groups"]["General"][modeIndex].erase("default");
        Reject(document, "default must be a string matching an option");
        const std::vector<Json> invalidValues{ true, 1, 1.0, nullptr, "", "Normal", "removed", Json::array(), Json::object() };
        for (const auto& value : invalidValues) {
            document = schema;
            document["groups"]["General"][modeIndex]["default"] = value;
            Reject(document, "default must be a string matching an option");
        }

        const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto root = fs::current_path() / "build" / "tests" / "enums" / run;
        const auto schemas = root / "schemas";
        const auto schemaFile = schemas / "learning.json";
        const auto values = root / "values";
        const auto valuesFile = values / "learning.json";
        const auto temporary = values / "learning.json.tmp";
        fs::create_directories(schemas);
        Write(schemaFile, schema.dump(2));
        OSFSettings::SettingsStore store;
        store.LoadAll(schemas, values);
        Check(store.LoadErrors().empty() && store.GetValue("learning", "notificationMode") == mode->DefaultValue(),
            "enum defaults load without a saved file");
        Check(store.Set("learning", "notificationMode", OSFSettings::EnumValue{"normal"}).ok && !fs::exists(values),
            "setting the current enum value does not write a file");
        Check(!store.Set("learning", "notifications", std::string{"true"}).ok &&
            !store.Set("learning", "notificationLimit", std::string{"3"}).ok &&
            !store.Set("learning", "notificationVolume", std::string{"0.75"}).ok && !fs::exists(values),
            "text values do not enable string coercion for other types");

        OSFSettings::SettingsStore restarted;
        for (const auto* value : { "quiet", "normal", "verbose" }) {
            Check(store.Set("learning", "notificationMode", OSFSettings::EnumValue{value}).ok, "each declared enum option can be selected");
            restarted.LoadAll(schemas, values);
            const auto saved = Json::parse(Read(valuesFile));
            Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationMode") == SettingValue{ OSFSettings::EnumValue{value} } &&
                saved["values"]["notificationMode"].is_string() && saved["values"]["notificationMode"] == value,
                "enum saving and reloading preserve the option string");
        }
        const auto committed = Read(valuesFile);
        for (const auto& value : std::vector<SettingValue>{ true, std::int64_t{1}, 1.0,
            std::string{"normal"}, OSFSettings::EnumValue{}, OSFSettings::EnumValue{"Normal"}, OSFSettings::EnumValue{"removed"} }) {
            const auto result = store.Set("learning", "notificationMode", value);
            Check(!result.ok && !result.error.empty() && Read(valuesFile) == committed &&
                store.GetValue("learning", "notificationMode") == SettingValue{ OSFSettings::EnumValue{"verbose"} },
                "wrong types, display labels, and unknown enum values preserve the live value and saved file");
        }
        fs::create_directory(temporary);
        Check(!store.Set("learning", "notificationMode", OSFSettings::EnumValue{"quiet"}).ok && Read(valuesFile) == committed &&
            store.GetValue("learning", "notificationMode") == SettingValue{ OSFSettings::EnumValue{"verbose"} },
            "a failed enum save preserves the live value and saved file");
        fs::remove(temporary); // Only the empty directory created by this test.
        Check(store.Set("learning", "notificationMode", mode->DefaultValue()).ok, "an enum resets through the normal save path");
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationMode") == mode->DefaultValue(),
            "the reset enum survives reload");
        Check(store.Set("learning", "notificationMode", OSFSettings::EnumValue{"verbose"}).ok &&
            store.Set("learning", "notifications", false).ok && store.Set("learning", "notificationLimit", std::int64_t{7}).ok &&
            store.Set("learning", "notificationVolume", 0.5).ok, "all four setting types can be saved together");
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationMode") == SettingValue{ OSFSettings::EnumValue{"verbose"} } &&
            restarted.GetValue("learning", "notifications") == SettingValue{ false } &&
            restarted.GetValue("learning", "notificationLimit") == SettingValue{ std::int64_t{7} } &&
            restarted.GetValue("learning", "notificationVolume") == SettingValue{ 0.5 }, "mixed edits preserve neighboring values and types");

        const auto savedSelection = Read(valuesFile);
        document = schema;
        auto& reordered = document["groups"]["General"][modeIndex];
        reordered["options"] = { { "verbose", "Detailed" }, { "quiet", "Minimal" }, { "normal", "Standard" } };
        Write(schemaFile, document.dump(2));
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().empty() && restarted.GetValue("learning", "notificationMode") == SettingValue{ OSFSettings::EnumValue{"verbose"} } &&
            Read(valuesFile) == savedSelection, "reordering options and changing labels preserve the saved selection without rewriting it");
        reordered["options"] = { "quiet", "normal" };
        Write(schemaFile, document.dump(2));
        restarted.LoadAll(schemas, values);
        Check(restarted.LoadErrors().size() == 1 && restarted.GetValue("learning", "notificationMode") == mode->DefaultValue() &&
            restarted.GetValue("learning", "notifications") == SettingValue{ false } && Read(valuesFile) == savedSelection,
            "a removed option falls back to the default and reports an error while preserving the file and valid neighbors");

        Write(schemaFile, schema.dump(2));
        for (const auto& value : invalidValues) {
            const Json saved = { { "formatVersion", 1 }, { "values", { { "notificationMode", value }, { "notifications", false } } } };
            Write(valuesFile, saved.dump());
            restarted.LoadAll(schemas, values);
            Check(restarted.GetValue("learning", "notificationMode") == mode->DefaultValue() &&
                restarted.GetValue("learning", "notifications") == SettingValue{ false } && restarted.LoadErrors().size() == 1 &&
                restarted.LoadErrors()[0].file == valuesFile && restarted.LoadErrors()[0].message.find("notificationMode") != std::string::npos &&
                Read(valuesFile) == saved.dump(), "invalid saved enums retain defaults, report their path and key, and preserve valid neighbors and the file");
        }
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
        document = example;
        document["id"] = "learning";
        Write(schemas / "mismatch.json", document.dump());

        store.LoadAll(schemas, values);
        Check(store.Mods().size() == 1 && store.LoadErrors().size() == 3,
            "valid schemas survive malformed, unsupported, and mismatched neighboring files");
        Check(std::ranges::all_of(store.LoadErrors(), &OSFSettings::SettingsLoadError::schema), "rejected schema files are marked as schema errors");
        Check(store.GetValue("learning", "notifications") == SettingValue{ true }, "the store owns the true default");
        Check(!store.GetValue("missing", "notifications").has_value(), "unknown mod returns no value");
        Check(!store.GetValue("learning", "missing").has_value(), "unknown key returns no value");
        Check(!store.GetValue("learning", "Notifications").has_value(), "setting keys are case-sensitive");

        auto ownedCopy = store.GetValue("learning", "notifications");
        ownedCopy = false;
        Check(ownedCopy == SettingValue{ false } && store.GetValue("learning", "notifications") == SettingValue{ true },
            "changing a returned copy does not edit the store");

        document = example;
        document["groups"]["General"][0]["default"] = false;
        Write(schemas / "learning.json", document.dump());
        store.LoadAll(schemas, values);
        Check(store.Mods().size() == 1 && store.GetValue("learning", "notifications") == SettingValue{ false },
            "reloading replaces defaults without duplicating mods; false is not a missing value");

        store.LoadAll(root / "missing", values);
        Check(store.Mods().empty() && store.LoadErrors().size() == 1 && store.LoadErrors()[0].schema,
            "a missing directory reports a schema error and clears stale values");
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
        schema["groups"]["General"].push_back({ { "key", "quiet" }, { "type", "bool" }, { "default", false } });
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
        const auto example = nlohmann::ordered_json::parse(input);
        TestStore(example, examplePath);
        TestPersistence(example);
        TestIntegers(example);
        TestFloats(example);
        TestEnums(example);
        TestFloatSlider(example);
        std::cout << checks - failures << '/' << checks << " checks passed\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Test setup failed: " << error.what() << '\n';
        return 1;
    }
}
