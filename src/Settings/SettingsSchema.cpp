#include "SettingsSchema.h"


namespace OSFSettings
{
    const SettingDefinition* ModSchema::FindSetting(std::string_view key) const
    {
        for(const auto& group : groups)
        {
            for(const auto& setting : group.settings)
            {
                if(setting.type != SettingType::Note && setting.key == key) {
                    return &setting;
                }
            }
        }
        return nullptr;
    }
}