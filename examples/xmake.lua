-- Development-only SDK example plugins. Each builds a small DLL from
-- examples/<dir>/main.cpp against the public sdk/ headers. None install or package.
local examples = {
    { dir = "diagnostics", basename = "OSFSettingsDiagnosticsExample", description = "Development-only Mod Issues SDK example" },
    { dir = "hotkeys",     basename = "OSFSettingsHotkeysExample",     description = "Development-only native hotkey SDK example" },
    { dir = "registry",    basename = "OSFSettingsRegistryExample",    description = "Development-only settings registry SDK example" },
    { dir = "actions",     basename = "OSFSettingsActionsExample",     description = "Development-only action buttons SDK example" },
}

for _, example in ipairs(examples) do
    target("osfsettings-" .. example.dir .. "-example")
        set_default(false)
        set_group("examples")
        set_basename(example.basename)
        add_rules("commonlibsf.plugin", {
            name = example.basename,
            author = "ozooma10",
            description = example.description,
            options = { address_library = false, no_struct_use = true }
        })
        add_files(example.dir .. "/main.cpp")
        add_includedirs("../sdk")
        set_values("commonlib.plugin.install", false)
        on_config(function(target)
            -- Keep the example out of installs and packages, including --all.
            target:set("installfiles", {})
        end)
    target_end()
end
