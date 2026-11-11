#include "Settings/SettingsJson.h"

#include <stdexcept>
#include <nlohmann/json.hpp>

int TestSettingsVersions()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    int checks{};
    const auto check = [&](bool passed, const char* message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    check(*SettingsVersion::Parse("1.10.0") > *SettingsVersion::Parse("1.9.0"), "minor versions compare numerically");
    check(*SettingsVersion::Parse("1.9.10") > *SettingsVersion::Parse("1.9.2"), "patch versions compare numerically");
    check(*SettingsVersion::Parse("2.0.0") > *SettingsVersion::Parse("1.65535.65535"), "major versions take precedence");
    for (const auto* text : { "0.0.0", "1.0.0", "65535.65535.65535" }) {
        const auto version = SettingsVersion::Parse(text);
        check(version && version->String() == text, "valid release versions round-trip");
    }
    const Json minimal = {{ "groups", Json::object() }};
    std::string error;
    for (const auto& invalid : { Json(nullptr), Json(true), Json(1.2), Json(1), Json::array(), Json::object(),
        Json(""), Json("1"), Json("1.2"), Json("1.2.3.4"), Json("1..3"), Json("1.2."), Json(".2.3"),
        Json("01.2.3"), Json("1.02.3"), Json("1.2.03"), Json("-1.2.3"), Json("+1.2.3"),
        Json("1.2.3-beta"), Json("1.2.3+build"), Json("v1.2.3"), Json(" 1.2.3"), Json("1.2.3 "),
        Json("65536.0.0"), Json("1.99999999999999999999.0"), Json(std::string("1.2.3\0", 6)) }) {
        auto document = minimal;
        document["version"] = invalid;
        check(!SettingsJson::ParseSchema(document, "sample", error) && error.find("version") != std::string::npos,
            "malformed release versions report a schema error");
    }
    auto legacy = minimal;
    legacy["expectedSettingsVersion"] = "9.9.9";
    check(SettingsJson::ParseSchema(legacy, "sample", error).has_value(), "the retired expectedSettingsVersion field is ignored");
    return checks;
}
