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

includes("tests")

-- define targets
target("OSF Settings")
    set_basename("OSFSettings")
    add_rules("commonlibsf.plugin", {
        name = "OSF Settings",
        author = "ozooma10",
        description = "Native MCM and Keybindings for Starfield",
        email = "ozooma10@protonmail.com"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "tests")
    set_pcxxheader("src/pch.h")
    add_packages("nlohmann_json")
    if has_config("test_harness") then
        add_defines("OSFSETTINGS_TEST_HARNESS")
        add_files("tests/harness/*.cpp")
        add_installfiles("build/papyrus/harness/*.pex", { prefixdir = "Scripts" })
        add_installfiles("build/papyrus/harness/OSFSettingsAcceptance.esm")
        add_installfiles("data/SFSE/Plugins/OSF/Settings/schemas/learning.json", { prefixdir = "SFSE/Plugins/OSF/Settings/schemas" })
    end
    add_headerfiles("tests/harness/*.h")
    before_build(function(target)
        local papyrusArgs = { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-papyrus.ps1") }
        if has_config("test_harness") then table.insert(papyrusArgs, "-TestHarness") end
        os.execv("pwsh", papyrusArgs)
        local args = { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-scaleform.ps1") }
        if has_config("test_harness") then table.insert(args, "-TestHarness") end
        os.execv("pwsh", args)
    end)
    add_installfiles("data/SFSE/Plugins/OSF/Settings/schemas/osfsettings.json", { prefixdir = "SFSE/Plugins/OSF/Settings/schemas" })
    add_installfiles("data/Scripts/Source/OSFSettings.psc", { prefixdir = "Scripts/Source" })
    add_installfiles("build/papyrus/OSFSettings.pex", { prefixdir = "Scripts" })
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

target("osfsettings-registry-example")
    set_default(false)
    set_basename("OSFSettingsRegistryExample")
    add_rules("commonlibsf.plugin", {
        name = "OSFSettingsRegistryExample",
        author = "ozooma10",
        description = "Development-only settings registry SDK example",
        options = { address_library = false, no_struct_use = true }
    })
    add_files("examples/registry/main.cpp")
    add_includedirs("sdk")
    set_values("commonlib.plugin.install", false)
    on_config(function(target)
        target:set("installfiles", {})
    end)

target("osfsettings-actions-example")
    set_default(false)
    set_basename("OSFSettingsActionsExample")
    add_rules("commonlibsf.plugin", {
        name = "OSFSettingsActionsExample",
        author = "ozooma10",
        description = "Development-only action buttons SDK example",
        options = { address_library = false, no_struct_use = true }
    })
    add_files("examples/actions/main.cpp")
    add_includedirs("sdk")
    set_values("commonlib.plugin.install", false)
    on_config(function(target)
        target:set("installfiles", {})
    end)
