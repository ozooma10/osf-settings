#pragma once

namespace OSFSettings::Localization
{
    // Starfield changes the string type prefix from s to S when an INI
    // override supplies a dynamic string. CommonLib's lookup is case-sensitive.
    template <class Collection>
    auto* FindLanguageSetting(Collection* ini)
    {
        auto* setting = ini ? ini->GetSetting("sLanguage:General") : nullptr;
        return setting || !ini ? setting : ini->GetSetting("SLanguage:General");
    }
}
