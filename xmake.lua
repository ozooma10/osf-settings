-- include subprojects
includes("lib/commonlibsf")

-- set project constants
set_project("OSF Settings Slim")
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

-- define targets
target("OSF Settings Slim")
    add_rules("commonlibsf.plugin", {
        name = "OSF Settings Slim",
        author = "ozooma10",
        description = "Mod Settings Menu for Starfield",
        email = "ozooma10@protonmail.com"
    })


    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_packages("nlohmann_json")
    before_build(function(target)
        os.execv("pwsh", { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-scaleform.ps1") })
    end)
    add_installfiles("data/(**)")
    add_installfiles("build/scaleform/OSFSettingsMenu.swf", "build/scaleform/OSFSettingsMenu_LRG.swf", { prefixdir = "Interface" })

target("osfsettings-slim-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/native/settings_tests.cpp", "src/Settings/*.cpp")
    add_includedirs("src")
    add_packages("nlohmann_json")
    set_rundir(os.projectdir())
