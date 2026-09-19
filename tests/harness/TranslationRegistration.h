#pragma once

#include <map>
#include <string>

namespace RE { class BSScaleformTranslator; }

namespace OSFSettings::TestHarness
{
    using TranslationLabels = std::map<std::wstring, std::wstring, std::less<>>;
#ifdef OSFSETTINGS_TEST_HARNESS
    void EnableTranslationRegistration(bool enabled);
    void BeforeTranslationLoad(RE::BSScaleformTranslator* translator);
    void BeforeTranslationRegistration(RE::BSScaleformTranslator* translator, const TranslationLabels& labels);
    void AfterTranslationRegistration(RE::BSScaleformTranslator* translator, bool registered);
    void ObserveTranslationRegistration(); // Existing menu callback / UI lane only.
#else
    inline void BeforeTranslationLoad(RE::BSScaleformTranslator*) {}
    inline void BeforeTranslationRegistration(RE::BSScaleformTranslator*, const TranslationLabels&) {}
    inline void AfterTranslationRegistration(RE::BSScaleformTranslator*, bool) {}
#endif
}
