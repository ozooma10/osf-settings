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
    set_basename("OSFSettingsSlim")
    -- This pinned CommonLibSF's plugin rule auto-installs unconditionally.
    -- Use a shared target so building remains separate from explicit deployment.
    set_kind("shared")
    add_deps("commonlibsf")
    local mods = os.getenv("XSE_SF_MODS_PATH")
    set_installdir(mods
        and path.join(mods, "OSF Settings Slim")
        or path.join(os.projectdir(), "build", "stage"))

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    add_packages("nlohmann_json")
    set_pcxxheader("src/pch.h")
    before_build(function(target)
        os.execv("pwsh", { "-NoProfile", "-File", path.join(os.projectdir(), "tools", "build-scaleform.ps1") })
    end)
    on_install(function(target)
        local install = target:installdir()
        local plugins = path.join(install, "SFSE", "Plugins")
        local interface = path.join(install, "Interface")
        os.mkdir(plugins)
        os.mkdir(interface)
        os.cp(target:targetfile(), path.join(plugins, "OSFSettingsSlim.dll"))
        if os.isfile(target:symbolfile()) then os.cp(target:symbolfile(), plugins) end
        os.cp(path.join(os.projectdir(), "data", "SFSE", "Plugins", "OSF"), plugins)
        for _, suffix in ipairs({ "", "_LRG" }) do
            local movie = "OSFSettingsMenu" .. suffix .. ".swf"
            os.cp(path.join(os.projectdir(), "build", "scaleform", movie), path.join(interface, movie))
        end
    end)

target("osfsettings-slim-tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/native/settings_tests.cpp", "src/Settings/*.cpp")
    add_includedirs("src")
    add_packages("nlohmann_json")
    set_rundir(os.projectdir())
