-- include subprojects
includes("lib/commonlibsf")

-- set project constants
set_project("OSF Settings")
set_version("1.1.1")
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

-- Generate build/generated/English.{h,as} from the shipped catalog and fail on string keys missing from it. The script only rewrites outputs that changed.
rule("osfsettings.localization")
    before_build(function(target)
        os.execv("python", { "-B", path.join(os.projectdir(), "tools", "generate-localization.py") })
    end)
rule_end()

includes("tests", "examples", "tools")

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
    add_includedirs("src", "build/generated")
    set_pcxxheader("src/pch.h")
    add_packages("nlohmann_json")
    if has_config("test_harness") then
        add_defines("OSFSETTINGS_TEST_HARNESS")
        add_files("tests/harness/*.cpp")
        add_headerfiles("tests/harness/*.h")
        add_installfiles("build/papyrus/harness/*.pex", { prefixdir = "Scripts" })
        add_installfiles("build/papyrus/harness/OSFSettingsAcceptance.esm")
        add_installfiles("data/SFSE/Plugins/OSF/Settings/schemas/learning.json", { prefixdir = "SFSE/Plugins/OSF/Settings/schemas" })
    end
    before_build(function(target)
        -- build-scaleform generates the shared English fallbacks before C++ compilation.
        local papyrusArgs = { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-papyrus.ps1") }
        if has_config("test_harness") then table.insert(papyrusArgs, "-TestHarness") end
        os.execv("pwsh", papyrusArgs)
        local args = { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-scaleform.ps1") }
        if has_config("test_harness") then table.insert(args, "-TestHarness") end
        os.execv("pwsh", args)
    end)
    add_installfiles("data/SFSE/Plugins/OSF/Settings/schemas/osfsettings.json", { prefixdir = "SFSE/Plugins/OSF/Settings/schemas" })
    add_installfiles("data/SFSE/Plugins/OSF/Settings/translations/en/osfsettings.json", { prefixdir = "SFSE/Plugins/OSF/Settings/translations/en" })
    add_installfiles("data/SFSE/Plugins/OSF/Settings/translations/ja/osfsettings.json", { prefixdir = "SFSE/Plugins/OSF/Settings/translations/ja" })
    add_installfiles("data/Scripts/Source/OSFSettings.psc", { prefixdir = "Scripts/Source" })
    add_installfiles("build/papyrus/OSFSettings.pex", { prefixdir = "Scripts" })
    add_installfiles("build/scaleform/OSFSettingsMenu.swf", "build/scaleform/OSFSettingsMenu_LRG.swf", { prefixdir = "Interface" })
