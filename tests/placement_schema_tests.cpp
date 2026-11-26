#include "Settings/SettingsJson.h"
#include "Settings/SettingsSchema.h"

#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

int TestPlacements()
{
    using namespace OSFSettings;
    using Json = nlohmann::ordered_json;
    int checks{};
    const auto check = [&](bool passed, const std::string& message) {
        if (!passed) throw std::runtime_error(message);
        ++checks;
    };
    std::string error;
    const Json settings = Json::array({
        {{ "key", "compassX" }, { "type", "int" }, { "default", 960 }, { "min", 0 }, { "max", 1920 }},
        {{ "key", "compassTop" }, { "type", "float" }, { "default", 24.0 }, { "min", 0.0 }, { "max", 1080.0 }},
        {{ "key", "compassScale" }, { "type", "float" }, { "default", 1.0 }, { "min", 0.25 }, { "max", 3.0 }},
        {{ "key", "panelWidth" }, { "type", "int" }, { "default", 400 }},
        {{ "key", "panelHeight" }, { "type", "int" }, { "default", 200 }},
        {{ "key", "label" }, { "type", "string" }, { "default", "" }},
        {{ "key", "negativeScale" }, { "type", "float" }, { "default", -1.0 }}
    });
    const Json compass = {
        { "type", "placement" }, { "id", "compass" }, { "label", "Compass" }, { "hint", "Drag the compass." },
        { "size", {{ "width", 400 }, { "height", 120 }} }, { "origin", {{ "x", 0.5 }, { "y", 0 }} },
        { "bind", {{ "x", "compassX" }, { "top", "compassTop" }, { "scale", "compassScale" }} }
    };
    const auto schema = [&](const Json& placement, bool placementFirst = false) {
        auto document = Json{{ "groups", Json::object() }};
        document["groups"]["HUD"] = placementFirst ? Json::array({ placement }) : settings;
        if (placementFirst) document["groups"]["Values"] = settings;
        else document["groups"]["HUD"].push_back(placement);
        return document;
    };
    const auto rejects = [&](const Json& placement, const std::string& expected, const std::string& message) {
        const auto parsed = SettingsJson::ParseSchema(schema(placement), "placements", error);
        check(!parsed && error.find(expected) != std::string::npos, message + " (got: " + error + ")");
    };

    {
        const auto parsed = SettingsJson::ParseSchema(schema(compass), "placements", error);
        check(parsed.has_value(), "a placement bound to int and float settings loads: " + error);
        const auto& placement = std::get<PlacementDefinition>(parsed->groups[0].controls.back());
        check(placement.id == "compass" && placement.label == "Compass" && placement.hint == "Drag the compass.",
            "placement identity and text are preserved");
        check(placement.screenWidth == 1920.0 && placement.screenHeight == 1080.0, "the screen defaults to 1920 x 1080");
        check(placement.width == 400.0 && placement.height == 120.0 && placement.originX == 0.5 && placement.originY == 0.0,
            "size and origin are read");
        check(placement.resize == PlacementResize::Scale, "binding scale defaults to proportional resizing");
        check(placement.Binding(PlacementRole::X) == "compassX" && placement.Binding(PlacementRole::Top) == "compassTop" &&
            placement.Binding(PlacementRole::Scale) == "compassScale" && placement.Binding(PlacementRole::Left).empty(),
            "bindings are stored by role");
        check(!parsed->FindSetting("compass") && parsed->DefaultValues().size() == settings.size(),
            "a placement has no value of its own");
    }
    {
        // Bindings may name settings declared after the placement, in a later group.
        const auto parsed = SettingsJson::ParseSchema(schema(compass, true), "placements", error);
        check(parsed.has_value(), "bindings resolve against the whole schema: " + error);
    }
    {
        auto free = compass;
        free.erase("size");
        free["screen"] = {{ "width", 2560 }, { "height", 1080 }};
        free["bind"] = {{ "left", "compassX" }, { "bottom", "compassTop" }, { "width", "panelWidth" }, { "height", "panelHeight" }};
        const auto parsed = SettingsJson::ParseSchema(schema(free), "placements", error);
        check(parsed.has_value(), "binding width and height makes size optional: " + error);
        const auto& placement = std::get<PlacementDefinition>(parsed->groups[0].controls.back());
        check(placement.resize == PlacementResize::Free && placement.screenWidth == 2560.0, "width/height bindings default to free resizing");
        free["resize"] = "none";
        const auto fixed = SettingsJson::ParseSchema(schema(free), "placements", error);
        check(fixed && std::get<PlacementDefinition>(fixed->groups[0].controls.back()).resize == PlacementResize::None,
            "an explicit resize mode overrides the default");
    }
    {
        // Both margins of an axis carry its size, so a stretched panel needs no width binding.
        auto stretched = compass;
        stretched.erase("size");
        stretched["bind"] = {{ "left", "compassX" }, { "right", "panelWidth" }, { "top", "compassTop" }, { "bottom", "panelHeight" }};
        const auto parsed = SettingsJson::ParseSchema(schema(stretched), "placements", error);
        check(parsed && std::get<PlacementDefinition>(parsed->groups[0].controls.back()).resize == PlacementResize::Free,
            "both margins of each axis make size optional and allow free resizing: " + error);
        stretched["bind"]["scale"] = "compassScale";
        rejects(stretched, "bind either scale or a size", "scale cannot be combined with margins that carry the size");
    }
    {
        auto moveOnly = compass;
        moveOnly["bind"] = {{ "right", "compassX" }};
        const auto parsed = SettingsJson::ParseSchema(schema(moveOnly), "placements", error);
        check(parsed && std::get<PlacementDefinition>(parsed->groups[0].controls.back()).resize == PlacementResize::None,
            "a placement with only position bindings cannot resize");
    }

    auto bad = compass;
    bad["bind"] = {{ "x", "missing" }};
    rejects(bad, "names no setting in this schema: missing", "bindings must name settings in the same schema");
    bad["bind"] = {{ "x", "label" }};
    rejects(bad, "must name an int or float setting: label", "bindings must name numeric settings");
    bad["bind"] = {{ "x", "compassX" }, { "left", "compassX" }};
    rejects(bad, "more than one role", "one setting cannot fill two roles");
    bad["bind"] = {{ "x", "compassX" }, { "center", "compassTop" }};
    rejects(bad, "unknown placement role 'center'", "unknown roles are rejected");
    bad["bind"] = Json::object();
    rejects(bad, "bind must be a non-empty object", "a placement must bind something");
    bad["bind"] = {{ "scale", "panelWidth" }};
    rejects(bad, "must name a float setting: panelWidth", "scale must be a float setting");
    bad["bind"] = {{ "scale", "negativeScale" }};
    rejects(bad, "must allow a positive scale", "scale must default to a positive value");
    bad["bind"] = {{ "scale", "compassScale" }, { "width", "panelWidth" }};
    rejects(bad, "bind either scale or a size", "scale cannot be combined with width or height");
    bad = compass;
    bad.erase("size");
    rejects(bad, "size is required", "size is required without width and height bindings");
    bad = compass;
    bad["resize"] = "free";
    rejects(bad, "resize free needs a size bound", "free resizing needs a size binding");
    bad["resize"] = "stretch";
    rejects(bad, "resize must be", "unknown resize modes are rejected");
    bad = compass;
    bad["origin"] = {{ "x", 1.5 }, { "y", 0 }};
    rejects(bad, "origin x and y must be fractions", "origins are fractions of the size");
    bad = compass;
    bad["screen"] = {{ "width", 1920 }};
    rejects(bad, "screen must have numeric width and height", "screens need both dimensions");
    bad["screen"] = {{ "width", 10000 }, { "height", 100 }};
    rejects(bad, "aspect ratio", "extreme aspect ratios are rejected");
    bad = compass;
    bad["size"] = {{ "width", 0 }, { "height", 10 }};
    rejects(bad, "size width and height must be positive", "sizes must be positive");
    for (const auto* field : { "key", "default", "requires", "group" }) {
        bad = compass;
        bad[field] = "restart";
        rejects(bad, field, std::string("placements reject the setting field ") + field);
    }
    bad = compass;
    bad["id"] = "has space";
    rejects(bad, "placement id must use", "placement ids are identifiers");
    {
        auto twice = schema(compass);
        auto copy = compass;
        copy["id"] = "COMPASS";
        twice["groups"]["HUD"].push_back(copy);
        check(!SettingsJson::ParseSchema(twice, "placements", error) && error == "duplicate placement id: COMPASS",
            "placement ids are unique ignoring ASCII case");
        auto shared = schema(compass);
        shared["groups"]["HUD"].push_back({{ "type", "action" }, { "id", "compass" }, { "label", "Run" }});
        check(SettingsJson::ParseSchema(shared, "placements", error).has_value(),
            "placements and actions are separate row types, so they may share an id: " + error);
    }
    return checks;
}
