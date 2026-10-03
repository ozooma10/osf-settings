target("osfsettings-preview-rows")
    set_kind("binary")
    set_default(false)
    set_group("tools")
    set_targetdir("../build/preview")
    add_rules("osfsettings.localization")
    add_deps("commonlibsf")
    add_packages("nlohmann_json")
    add_includedirs("../src", "../build/generated")
    add_files("preview-rows.cpp", "../src/Input/KeyNames.cpp",
        "../src/Settings/SettingsSchema.cpp", "../src/Settings/SettingsSchemaJson.cpp",
        "../src/Settings/SettingsJson.cpp", "../src/Settings/SettingsVersion.cpp",
        "../src/Settings/Localization.cpp", "../src/Menu/FloatSlider.cpp", "../src/Menu/SettingRow.cpp")
target_end()
