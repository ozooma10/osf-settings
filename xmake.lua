-- include subprojects
includes("lib/commonlibsf")

-- set project constants
set_project("OSF Settings Slim")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate", { outputdir = ".", lsp = "cpptools" })

-- define targets
target("OSF Settings Slim")
    add_rules("commonlibsf.plugin", {
        name = "OSF Settings Slim",
        author = "ozooma10",
        description = "Mod Settings",
        email = "ozooma10@protonmail.com"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
