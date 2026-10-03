#include "SFSE/Impl/PCH.h"
#include "RE/B/BSInputDeviceManager.h"
#include "Menu/SettingRow.h"
#include "Settings/SettingsJson.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace
{
    std::wstring keyboardTable;
}

// The offline parser uses KeyNames.cpp's table fallback. Only that read-only
// relocation is available; no engine functions can run in this executable.
namespace RE
{
    BSInputDeviceManager* BSInputDeviceManager::GetSingleton() { return nullptr; }
}

namespace REL
{
    IDDB::IDDB() = default;

    std::uint64_t IDDB::offset(std::uint64_t id) const
    {
        if (id != RE::ID::BSWin32KeyboardDevice::KeyNameTable.id())
            throw std::runtime_error("Unexpected engine relocation in preview: " + std::to_string(id));
        return reinterpret_cast<std::uintptr_t>(keyboardTable.c_str()) - REX::FModule::GetExecutingModule().GetBaseAddress();
    }
}

int wmain(int argc, wchar_t** argv)
{
    try {
        if (argc < 3) throw std::runtime_error("Usage: osfsettings-preview-rows keyboard-table.utf16 schema.json [...]");
        std::ifstream table(std::filesystem::path(argv[1]), std::ios::binary | std::ios::ate);
        if (!table || table.tellg() <= 0 || table.tellg() % sizeof(wchar_t) != 0)
            throw std::runtime_error("Cannot read UTF-16 keyboard table");
        keyboardTable.resize(static_cast<std::size_t>(table.tellg()) / sizeof(wchar_t));
        table.seekg(0);
        if (!table.read(reinterpret_cast<char*>(keyboardTable.data()), keyboardTable.size() * sizeof(wchar_t)))
            throw std::runtime_error("Cannot read UTF-16 keyboard table");

        nlohmann::json output{ { "rows", nlohmann::json::array() }, { "titles", nlohmann::json::object() } };
        for (int index = 2; index < argc; ++index) {
            const std::filesystem::path path(argv[index]);
            std::ifstream input(path);
            if (!input) throw std::runtime_error("Cannot open schema: " + path.string());
            std::string error;
            const auto schema = OSFSettings::SettingsJson::ParseSchema(input, path.stem().string(), error);
            if (!schema) throw std::runtime_error(path.string() + ": " + error);
            output["titles"][schema->id] = schema->title;
            for (const auto& group : schema->groups) {
                for (const auto& control : group.controls) {
                    nlohmann::json row{
                        { "mod", schema->id }, { "modTitle", schema->title }, { "modDescription", schema->description },
                        { "group", group.id }, { "groupTitle", group.label }
                    };
                    if (const auto* setting = std::get_if<OSFSettings::SettingDefinition>(&control)) {
                        row["key"] = setting->key;
                        row["title"] = setting->label;
                        row["hint"] = setting->hint;
                        const auto presentation = OSFSettings::MakeSettingRow(*setting, setting->DefaultValue());
                        for (const auto& [name, field] : presentation.fields)
                            std::visit([&](const auto& value) { row[name] = value; }, field);
                        if (!presentation.options.empty()) {
                            row["options"] = nlohmann::json::array();
                            for (const auto& option : presentation.options)
                                row["options"].push_back({ { "value", option.value }, { "label", option.label } });
                        }
                    } else {
                        const auto& action = std::get<OSFSettings::ActionDefinition>(control);
                        row.update({ { "key", action.id }, { "title", action.label }, { "hint", action.hint },
                            { "confirmation", action.confirmation }, { "type", "action" },
                            { "editable", true }, { "actionState", "Run" }, { "message", "Ready." } });
                    }
                    output["rows"].push_back(std::move(row));
                }
            }
        }
        std::cout << output.dump() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
