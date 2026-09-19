#include "TranslationRegistration.h"
#include "RE/B/BSScaleformManager.h"
#include "RE/B/BSScaleformTranslator.h"
#include "REX/CONVERT.h"
#include <Windows.h>
#undef ERROR
#include <nlohmann/json.hpp>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace OSFSettings::TestHarness
{
    namespace
    {
        using Translator = RE::BSScaleformTranslator;
        using Json = nlohmann::json;
        bool g_enabled{}, g_ready{};
        TranslationLabels g_labels, g_expected;
        std::unordered_map<const wchar_t*, RE::BSStringPool::Entry*> g_stock;
        std::mutex g_mutex;
        Json g_report = {{"enabled", false}};
        Json g_registration;
        std::uint64_t g_generation{}, g_lastObservation{};

        template <class T> T Read(const void* object, std::size_t offset)
        {
            T result;
            std::memcpy(&result, static_cast<const std::byte*>(object) + offset, sizeof(result));
            return result;
        }
        void Require(bool condition, const char* message)
        {
            if (!condition) throw std::runtime_error(message);
        }
        void Publish()
        {
            std::scoped_lock lock(g_mutex);
            g_report = g_registration;
        }
        void Fail(const std::exception& error)
        {
            g_ready = false;
            g_registration["error"] = error.what();
            REX::ERROR("Translation registration checks failed: {}", error.what());
            Publish();
        }
        std::string UTF8(std::wstring_view text)
        {
            std::string result;
            Require(REX::UTF16_TO_UTF8(text, result), "invalid UTF-16 result");
            return result;
        }
        auto Index(const Translator* translator)
        {
            Require(translator->entries && translator->capacity && translator->capacity <= 1048576 && translator->free <= translator->capacity,
                "invalid translation table bounds");
            std::unordered_map<const wchar_t*, const Translator::Entry*> nodes;
            for (const auto& node : std::span(translator->entries, translator->capacity)) {
                if (node.nextIndex != -1) {
                    Require(!node.key.empty() && node.nextIndex >= 0 && static_cast<std::uint64_t>(node.nextIndex) <= translator->capacity,
                        "invalid occupied translation node");
                    Require(nodes.emplace(node.key.c_str(), &node).second, "duplicate canonical translation key");
                }
            }
            Require(nodes.size() == translator->capacity - translator->free, "translation occupancy mismatch");
            return nodes;
        }
        void RegisterLabels(Translator* translator, const TranslationLabels& labels)
        {
            std::vector<Translator::Translation> translations;
            for (const auto& [key, value] : labels) translations.push_back({ key.c_str(), value.c_str() });
            Require(translator->RegisterTranslations(translations), "CommonLib translation registration failed");
        }
    }

    void EnableTranslationRegistration(bool enabled)
    {
        g_enabled = enabled;
        std::scoped_lock lock(g_mutex);
        g_report = {{"enabled", enabled}};
    }

    void BeforeTranslationLoad(Translator* translator)
    {
        if (!g_enabled) return;
        g_ready = true;
        g_registration = {{"enabled", true}, {"installed", true}, {"passed", false}, {"generation", ++g_generation},
            {"thread", ::GetCurrentThreadId()}, {"enteredAtMs", ::GetTickCount64()}, {"lookupOverridesDisabled", true}};
        try {
            const auto* manager = RE::BSScaleformManager::GetSingleton();
            Require(manager && !manager->GetTranslator(), "translator was already published");
            Require(translator && translator->impl && translator->impl->translations == reinterpret_cast<std::byte*>(translator) + 8,
                "impl does not borrow the constructing wrapper map");
            Require(Read<std::uintptr_t>(translator->impl, 0) == RE::VTABLE::BSScaleformTranslator__ScaleformImpl[0].address(),
                "unexpected translator vtable");
            g_registration["wrapper"] = reinterpret_cast<std::uintptr_t>(translator);
            g_registration["impl"] = reinterpret_cast<std::uintptr_t>(translator->impl);
            g_registration["prepublication"] = true;
            g_registration["initialCapacity"] = translator->capacity;

            Require(translator->capacity == 8 && translator->free == 8, "initial table is not empty capacity eight");
            std::wstring growth;
            for (unsigned i = 0; i < 33; ++i) growth += L"$OSFTranslationProbe_Growth" + std::to_wstring(i) + L"\tfirst\n";
            growth += L"$OSFTranslationProbe_Growth0\tlast\n";
            Require(translator->LoadTranslations(growth), "native loader did not finish memory stream");
            Require(translator->capacity > 8 && Index(translator).size() == 33, "growth or duplicate occupancy failed");
            Require(std::wstring_view(translator->impl->Translate(L"$OSFTranslationProbe_Growth0").c_str()) == L"last",
                "duplicate did not replace value");
            g_registration["growthCapacity"] = translator->capacity;
            g_registration["growthAndDuplicate"] = true;
        } catch (const std::exception& error) { Fail(error); }
    }

    void BeforeTranslationRegistration(Translator* translator, const TranslationLabels& labels)
    {
        if (!g_enabled || !g_ready) return;
        try {
            g_registration["stockLoadedAtMs"] = ::GetTickCount64();
            g_labels = labels;
            g_expected = labels;
            g_expected.emplace(L"$OSFTranslationProbe_Unicode", L"Caf\u00e9: \u65e5\u672c\u8a9e e\u0301 \U0001f680");
            g_expected.emplace(L"$OSFTranslationProbe_Exact", L"\tCR\rLF\nTAB\t literal\\n $dollar <tag> Caf\u00e9");
            g_expected.emplace(L"$OSFTranslationProbe_Long", std::wstring(700, L'\u00e9'));
            g_stock.clear();
            for (const auto& [key, node] : Index(translator)) g_stock.emplace(key, Read<RE::BSStringPool::Entry*>(&node->value, 0));
            g_registration["vanillaForward"] = UTF8(translator->impl->Translate(L"$MainGameplay_Forward").c_str());
        } catch (const std::exception& error) { Fail(error); }
    }

    void AfterTranslationRegistration(Translator* translator, bool registered)
    {
        if (!g_enabled || !g_ready) return;
        try {
            Require(registered, "production translation registration failed");
            // Check production output before replaying it or adding diagnostic keys.
            for (const auto& [key, value] : g_labels) {
                Require(std::wstring_view(translator->impl->Translate(key.c_str()).c_str()) == value, "production label differs from schema");
            }
            RegisterLabels(translator, g_expected);
            const auto count = translator->capacity - translator->free;
            RegisterLabels(translator, g_expected);
            Require(translator->capacity - translator->free == count, "registration replay added duplicate nodes");
            const auto current = Index(translator);
            const RE::BSFixedStringWCS probeKey(L"$OSFTranslationProbe_Exact");
            const auto probe = current.find(probeKey.c_str());
            Require(probe != current.end(), "reference-count probe key missing");
            const auto* probeValue = Read<RE::BSStringPool::Entry*>(&probe->second->value, 0);
            const auto references = Read<std::uint32_t>(probeValue, 0x10);
            {
                const RE::BSFixedStringWCS retained(probeValue->data<wchar_t>());
                Require(Read<RE::BSStringPool::Entry*>(&retained, 0) == probeValue &&
                    Read<std::uint32_t>(probeValue, 0x10) == references + 1, "pooled value acquire differs from expected ownership");
            }
            Require(Read<std::uint32_t>(probeValue, 0x10) == references, "pooled value reference was not released");
            for (const auto& [key, value] : g_stock) {
                const auto found = current.find(key);
                Require(found != current.end() && Read<RE::BSStringPool::Entry*>(&found->second->value, 0) == value,
                    "registration altered an existing translation");
            }
            for (const auto& [key, value] : g_expected) {
                Require(std::wstring_view(translator->impl->Translate(key.c_str()).c_str()) == value, "helper output differs from exact UTF-16 label");
            }
            const auto translate = [&](const wchar_t* key) { return std::wstring(translator->impl->Translate(key).c_str()); };
            Require(translate(L"$OSFTranslationProbe_Unknown") == L"$OSFTranslationProbe_Unknown", "unknown key fallback changed");
            Require(translate(L"$osfmodbindings") == L"$osfmodbindings", "case-sensitive fallback changed");
            Require(translate(L"$$OSFModBindings *") == L"Mod Bindings *", "required-row token parsing changed");
            Require(translate(L"$OSFTranslationProbe_Growth0") == L"last", "stock resource reload lost the probe key");
            g_registration["referenceOwnership"] = true;
            g_registration["preservedEntries"] = g_stock.size();
            g_registration["checkedLabels"] = g_expected.size();
            g_registration["schemaLabels"] = g_labels.size();
            g_registration["replayWithoutDuplicates"] = true;
            g_registration["passed"] = true;
            g_registration["registeredAtMs"] = ::GetTickCount64();
            REX::INFO("Translation registration checks: generation={} schemaLabels={} preserved={} passed", g_generation, g_labels.size(), g_stock.size());
            Publish();
        } catch (const std::exception& error) { Fail(error); }
    }

    void ObserveTranslationRegistration()
    {
        if (!g_enabled || ::GetTickCount64() - g_lastObservation < 1000) return;
        g_lastObservation = ::GetTickCount64();
        Json observation = {{"passed", false}, {"observedAtMs", g_lastObservation}, {"thread", ::GetCurrentThreadId()}};
        try {
            auto* manager = RE::BSScaleformManager::GetSingleton();
            Require(manager, "manager unavailable after publication");
            const auto* translator = manager->GetTranslator();
            Require(translator && translator->impl, "published translator unavailable");
            observation["wrapper"] = reinterpret_cast<std::uintptr_t>(translator);
            observation["impl"] = reinterpret_cast<std::uintptr_t>(translator->impl);
            for (const auto& [key, value] : g_expected) {
                const auto result = manager->Translate(key.c_str());
                Require(std::wstring_view(result.c_str()) == value, "native manager output differs from registered label");
            }
            observation["heading"] = UTF8(manager->Translate(L"$OSFModBindings").c_str());
            observation["action"] = UTF8(manager->Translate(L"$OSFModBindings_osfsettings/openMenu").c_str());
            observation["unicode"] = UTF8(manager->Translate(L"$OSFTranslationProbe_Unicode").c_str());
            observation["unknown"] = UTF8(manager->Translate(L"$OSFTranslationProbe_Unknown").c_str());
            observation["vanillaForward"] = UTF8(manager->Translate(L"$MainGameplay_Forward").c_str());
            observation["checkedLabels"] = g_expected.size();
            observation["passed"] = true;
        } catch (const std::exception& error) { observation["error"] = error.what(); }
        std::scoped_lock lock(g_mutex);
        g_report["native"] = std::move(observation);
    }

    nlohmann::json TranslationRegistrationSnapshot()
    {
        std::scoped_lock lock(g_mutex);
        return g_report;
    }
}
