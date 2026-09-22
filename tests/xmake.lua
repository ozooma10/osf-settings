-- Separate executables isolate each fixture's engine stand-ins and global state.
local function test_target(name)
    target(name)
    set_kind("binary")
    set_default(false)
    set_rundir(os.projectdir())
    add_tests("default")
end

test_target("osfsettings-tests")
    add_deps("commonlibsf")
    add_files("native/*.cpp", "../src/Settings/*.cpp", "../src/API/*.cpp", "../src/Actions/*.cpp", "../src/Diagnostics/*.cpp", "../src/Input/KeyNames.cpp", "../src/Input/KeyCapture.cpp", "../src/Input/HotkeyService.cpp", "../src/Menu/FloatSlider.cpp")
    add_files("../src/Launcher/LauncherService.cpp")
    add_includedirs("../src")
    add_packages("nlohmann_json")

test_target("osfsettings-schema-tests")
    add_files("hotkey_schema_tests.cpp", "../src/Settings/SettingsSchema.cpp", "../src/Settings/SettingsSchemaJson.cpp",
        "../src/Settings/SettingsJson.cpp", "../src/Settings/SettingsStore.cpp", "../src/Settings/SettingsValuesJson.cpp")
    add_includedirs("../src")
    add_packages("nlohmann_json")

test_target("osfsettings-launcher-tests")
    add_deps("commonlibsf")
    add_files("launcher_tests.cpp", "../src/Launcher/LauncherService.cpp",
        "../src/API/LauncherApi.cpp", "../src/Settings/SettingsSchema.cpp")
    add_includedirs("../src", "../sdk")
    set_pcxxheader("../src/pch.h")
    add_packages("nlohmann_json")

test_target("osfsettings-action-tests")
    add_deps("commonlibsf")
    add_files("action_tests.cpp", "../src/Actions/ActionService.cpp", "../src/API/ActionsApi.cpp", "../src/Papyrus/Actions.cpp",
        "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp", "../src/API/SettingsApi.cpp", "../src/API/Exports.cpp",
        "../src/Settings/*.cpp")
    add_includedirs("../src", "../sdk")
    set_pcxxheader("../src/pch.h")
    add_packages("nlohmann_json")

test_target("osfsettings-issues-tests")
    add_files("mod_issue_tests.cpp", "diagnostics_service_tests.cpp", "../src/Diagnostics/*.cpp", "../src/Settings/SettingsSchema.cpp")
    add_includedirs("../src")

test_target("osfsettings-registration-tests")
    add_deps("commonlibsf")
    add_files("hotkey_registration_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp", "../src/Input/KeyNames.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")

test_target("osfsettings-input-tests")
    add_deps("commonlibsf")
    add_files("native_input_user_tests.cpp", "../src/Input/BSInputEventUserStandalone.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")

test_target("osfsettings-lifecycle-tests")
    add_deps("commonlibsf")
    add_files("hotkey_lifecycle_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp", "../src/Input/BSInputEventUserStandalone.cpp")
    add_includedirs("../src", ".")
    add_defines("OSFSETTINGS_TEST_HARNESS")
    set_pcxxheader("../src/pch.h")

test_target("osfsettings-binding-tests")
    add_deps("commonlibsf")
    add_files("native_binding_editor_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")

test_target("osfsettings-bindings-menu-tests")
    add_deps("commonlibsf")
    add_files("native_bindings_menu_tests.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")

test_target("osfsettings-binding-snapshot-tests")
    add_deps("commonlibsf")
    add_files("binding_snapshot_tests.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")

test_target("osfsettings-diagnostics-api-tests")
    add_deps("commonlibsf")
    add_files("diagnostics_api_tests.cpp", "../src/API/DiagnosticsApi.cpp", "../src/API/DiagnosticsExports.cpp",
        "../src/Diagnostics/*.cpp", "../src/Settings/SettingsSchema.cpp")
    add_includedirs("../src")

test_target("osfsettings-hotkey-block-tests")
    add_deps("commonlibsf")
    add_files("hotkey_block_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp", "../src/Input/KeyNames.cpp",
        "../src/API/SettingsApi.cpp", "../src/API/Exports.cpp", "../src/API/ActionsApi.cpp", "../src/Actions/ActionService.cpp", "../src/Settings/*.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")
    add_packages("nlohmann_json")

test_target("osfsettings-hotkey-callback-tests")
    add_files("hotkey_callback_tests.cpp", "../src/Input/HotkeyInputState.cpp")
    add_includedirs("stubs", "../src")

test_target("osfsettings-string-tests")
    add_deps("commonlibsf")
    add_files("string_settings_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp", "../src/Input/KeyNames.cpp",
        "../src/API/SettingsApi.cpp", "../src/API/Exports.cpp", "../src/API/ActionsApi.cpp", "../src/Actions/ActionService.cpp", "../src/Settings/*.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")
    add_packages("nlohmann_json")

test_target("osfsettings-registry-tests")
    add_deps("commonlibsf")
    add_files("registry_api_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp", "../src/Input/KeyNames.cpp",
        "../src/API/SettingsApi.cpp", "../src/API/Exports.cpp", "../src/API/ActionsApi.cpp", "../src/Actions/ActionService.cpp", "../src/Settings/*.cpp")
    add_includedirs("../src", "../sdk", "../examples/registry")
    set_pcxxheader("../src/pch.h")
    add_packages("nlohmann_json")

test_target("osfsettings-papyrus-tests")
    add_deps("commonlibsf")
    add_files("papyrus_tests.cpp", "HotkeyTasks.cpp", "../src/Input/HotkeyInputState.cpp",
        "../src/Papyrus/Values.cpp", "../src/Papyrus/Subscriptions.cpp", "../src/Input/KeyNames.cpp", "../src/Settings/*.cpp")
    add_includedirs("../src")
    set_pcxxheader("../src/pch.h")
    add_packages("nlohmann_json")
