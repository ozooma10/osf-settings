-- Shared production objects; separate executables preserve fixture isolation.
target("osfsettings-test-core")
    set_kind("static")
    set_default(false)
    set_group("tests/support")
    add_rules("osfsettings.localization")
    add_includedirs("../src", "../build/generated", { public = true })
    add_packages("nlohmann_json", { public = true })
    add_files("../src/Settings/*.cpp|SettingsService.cpp", "../src/Persistence/AtomicFile.cpp",
        "../src/Menu/FloatSlider.cpp", "../src/Diagnostics/*.cpp")
target_end()

target("osfsettings-test-services")
    set_kind("static")
    set_default(false)
    set_group("tests/support")
    add_deps("osfsettings-test-core", "commonlibsf", { public = true })
    add_files("../src/Settings/SettingsService.cpp", "../src/API/*.cpp", "../src/Actions/*.cpp",
        "../src/Launcher/LauncherService.cpp", "../src/Input/KeyNames.cpp", "../src/Input/KeyCapture.cpp",
        "../src/Input/HotkeyInputState.cpp", "../src/Input/BSInputEventUserStandalone.cpp",
        "../src/Papyrus/Actions.cpp", "../src/Papyrus/Values.cpp", "../src/Papyrus/Subscriptions.cpp",
        "../src/Papyrus/Issues.cpp")
    -- Use CommonLib's foundation once, instead of RE/Starfield.h per fixture.
    set_pcxxheader("../lib/commonlibsf/include/SFSE/Impl/PCH.h")
target_end()

target("osfsettings-test-tasks")
    set_kind("static")
    set_default(false)
    set_group("tests/support")
    add_deps("commonlibsf", { public = true })
    add_files("HotkeyTasks.cpp")
target_end()

local function test_target(name, group, engine)
    target(name)
    set_kind("binary")
    set_default(false)
    set_group("tests/" .. group)
    set_rundir(os.projectdir())
    add_tests("default")
    if engine then
        add_deps("osfsettings-test-services", "osfsettings-test-tasks")
        add_forceincludes("SFSE/Impl/PCH.h")
    else
        add_deps("osfsettings-test-core")
    end
end

test_target("osfsettings-localization-tests", "settings")
    add_files("localization_tests.cpp")

test_target("osfsettings-schema-tests", "settings")
    add_files("hotkey_schema_tests.cpp")

test_target("osfsettings-store-tests", "settings")
    add_files("native/settings_tests.cpp")

test_target("osfsettings-service-tests", "settings", true)
    add_files("native/service_tests.cpp")

test_target("osfsettings-key-tests", "input", true)
    add_files("native/key_tests.cpp", "native/engine_string_stubs.cpp")

test_target("osfsettings-sdk-tests", "settings", true)
    add_files("native/sdk_tests.cpp")

test_target("osfsettings-launcher-tests", "launcher", true)
    add_files("launcher_tests.cpp")
    add_includedirs("../sdk")

test_target("osfsettings-action-tests", "actions", true)
    add_files("action_tests.cpp")
    add_includedirs("../sdk")

test_target("osfsettings-issues-tests", "diagnostics")
    add_files("mod_issue_tests.cpp", "diagnostics_service_tests.cpp")

test_target("osfsettings-registration-tests", "input", true)
    add_files("hotkey_registration_tests.cpp")

test_target("osfsettings-input-tests", "input", true)
    add_files("native_input_user_tests.cpp")

test_target("osfsettings-lifecycle-tests", "input", true)
    add_files("hotkey_lifecycle_tests.cpp")
    add_includedirs(".")
    add_defines("OSFSETTINGS_TEST_HARNESS")

test_target("osfsettings-binding-tests", "input", true)
    add_files("native_binding_editor_tests.cpp")

test_target("osfsettings-bindings-menu-tests", "input", true)
    add_files("native_bindings_menu_tests.cpp")

test_target("osfsettings-binding-snapshot-tests", "input", true)
    add_files("binding_snapshot_tests.cpp")

test_target("osfsettings-diagnostics-api-tests", "diagnostics", true)
    add_files("diagnostics_api_tests.cpp")

test_target("osfsettings-hotkey-block-tests", "input", true)
    add_files("hotkey_block_tests.cpp")

-- This fixture intentionally uses its own SFSE stub, not the real task wrapper.
target("osfsettings-hotkey-callback-tests")
    set_kind("binary")
    set_default(false)
    set_group("tests/input")
    set_rundir(os.projectdir())
    add_tests("default")
    add_files("hotkey_callback_tests.cpp", "../src/Input/HotkeyInputState.cpp")
    add_includedirs("stubs", "../src")

test_target("osfsettings-string-tests", "settings", true)
    add_files("string_settings_tests.cpp")

test_target("osfsettings-registry-tests", "settings", true)
    add_files("registry_api_tests.cpp")
    add_includedirs("../sdk", "../examples/registry")

test_target("osfsettings-papyrus-tests", "papyrus", true)
    add_files("papyrus_tests.cpp")
target_end()
