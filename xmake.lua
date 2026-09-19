-- include subprojects
includes("lib/commonlibsf")

-- set project constants
set_project("OSF Settings")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")
add_requires("nlohmann_json 3.12.0")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate", { outputdir = ".", lsp = "cpptools" })

option("test_harness")
    set_default(false)
    set_showmenu(true)
    set_description("Expose passive local testing observations; normal input remains active")
option_end()

-- define targets
target("OSF Settings")
    set_basename("OSFSettings")
    add_rules("commonlibsf.plugin", {
        name = "OSF Settings",
        author = "ozooma10",
        description = "Mod Settings Menu for Starfield",
        email = "ozooma10@protonmail.com"
    })
    on_config(function(target)
        -- Keep this checkout's MO2 payload separate from the historical Settings mod.
        if os.getenv("XSE_SF_MODS_PATH") then
            target:set("installdir", path.join(os.getenv("XSE_SF_MODS_PATH"), "OSF Settings Slim"))
        end
    end)

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "tests")
    set_pcxxheader("src/pch.h")
    add_packages("nlohmann_json")
    if has_config("test_harness") then
        add_defines("OSFSETTINGS_TEST_HARNESS")
        add_files("tests/harness/*.cpp")
    end
    add_headerfiles("tests/harness/*.h")
    before_build(function(target)
        local args = { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-scaleform.ps1") }
        if has_config("test_harness") then table.insert(args, "-TestHarness") end
        os.execv("pwsh", args)
    end)
    add_installfiles("data/(**)")
    add_installfiles("build/scaleform/OSFSettingsMenu.swf", "build/scaleform/OSFSettingsMenu_LRG.swf", { prefixdir = "Interface" })

target("osfsettings-diagnostics-example")
    set_default(false)
    set_basename("OSFSettingsDiagnosticsExample")
    add_rules("commonlibsf.plugin", {
        name = "OSFSettingsDiagnosticsExample",
        author = "ozooma10",
        description = "Development-only Mod Issues SDK example",
        options = { address_library = false, no_struct_use = true }
    })
    add_files("examples/diagnostics/main.cpp")
    add_includedirs("sdk")
    set_values("commonlib.plugin.install", false)
    on_config(function(target)
        -- Keep the example out of installs and packages, including --all.
        target:set("installfiles", {})
    end)

target("osfsettings-hotkeys-example")
    set_default(false)
    set_basename("OSFSettingsHotkeysExample")
    add_rules("commonlibsf.plugin", {
        name = "OSFSettingsHotkeysExample",
        author = "ozooma10",
        description = "Development-only native hotkey SDK example",
        options = { address_library = false, no_struct_use = true }
    })
    add_files("examples/hotkeys/main.cpp")
    add_includedirs("sdk")
    set_values("commonlib.plugin.install", false)
    on_config(function(target)
        target:set("installfiles", {})
    end)

target("osfsettings-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/native/*.cpp", "src/Settings/*.cpp", "src/API/*.cpp", "src/Diagnostics/*.cpp", "src/Input/KeyNames.cpp", "src/Input/KeyCapture.cpp", "src/Input/HotkeyService.cpp", "src/Menu/FloatSlider.cpp")
    add_includedirs("src")
    add_packages("nlohmann_json")
    set_rundir(os.projectdir())

target("osfsettings-schema-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/hotkey_schema_tests.cpp", "src/Settings/SettingsSchema.cpp", "src/Settings/SettingsSchemaJson.cpp",
        "src/Settings/SettingsJson.cpp", "src/Settings/SettingsStore.cpp", "src/Settings/SettingsValuesJson.cpp")
    add_includedirs("src")
    add_packages("nlohmann_json")
    set_rundir(os.projectdir())

target("osfsettings-issues-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/mod_issue_tests.cpp", "tests/diagnostics_service_tests.cpp", "src/Diagnostics/*.cpp", "src/Settings/SettingsSchema.cpp")
    add_includedirs("src")
    set_rundir(os.projectdir())

target("osfsettings-registration-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/hotkey_registration_tests.cpp", "tests/HotkeyTasks.cpp", "src/Input/HotkeyInputState.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

target("osfsettings-input-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/native_input_user_tests.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

target("osfsettings-lifecycle-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/hotkey_lifecycle_tests.cpp", "tests/HotkeyTasks.cpp", "src/Input/HotkeyInputState.cpp")
    add_includedirs("src", "tests")
    add_defines("OSFSETTINGS_TEST_HARNESS")
    set_pcxxheader("src/pch.h")

target("osfsettings-binding-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/native_binding_editor_tests.cpp", "tests/HotkeyTasks.cpp", "src/Input/HotkeyInputState.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

target("osfsettings-bindings-menu-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/native_bindings_menu_tests.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

target("osfsettings-binding-snapshot-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/binding_snapshot_tests.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")

target("osfsettings-diagnostics-api-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/diagnostics_api_tests.cpp", "src/API/DiagnosticsApi.cpp", "src/API/DiagnosticsExports.cpp",
        "src/Diagnostics/*.cpp", "src/Settings/SettingsSchema.cpp")
    add_includedirs("src")
    set_rundir(os.projectdir())

target("osfsettings-hotkey-block-tests")
    set_kind("binary")
    set_default(false)
    add_deps("commonlibsf")
    add_files("tests/hotkey_block_tests.cpp", "tests/HotkeyTasks.cpp", "src/Input/HotkeyInputState.cpp", "src/Input/KeyNames.cpp",
        "src/API/SettingsApi.cpp", "src/API/Exports.cpp", "src/Settings/*.cpp")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_packages("nlohmann_json")
    set_rundir(os.projectdir())

target("osfsettings-hotkey-callback-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/hotkey_callback_tests.cpp", "src/Input/HotkeyInputState.cpp")
    add_includedirs("tests/stubs", "src")
    set_rundir(os.projectdir())
