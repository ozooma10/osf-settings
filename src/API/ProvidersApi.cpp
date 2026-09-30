#include "../../sdk/OSFSettings_Providers.h"
#include "Settings/SettingsService.h"
#include "Settings/SettingsJson.h"
#include "Input/KeyActions.h"
#include "REX/LOG.h"
#include <nlohmann/json.hpp>
#include <sstream>

namespace OSFSettings::API::Providers
{
    namespace
    {
        class ProvidersApi final : public IProviders
        {
            Status Register(const char* mod, const char* schemaJson, const char* valuesJson,
                SaveFn save, void* context, Registration* registration) noexcept override
            {
                if (!mod || !schemaJson || !valuesJson || !save || !registration) return Status::InvalidArgument;
                try {
                    std::istringstream input(schemaJson);
                    std::string error;
                    auto schema = SettingsJson::ParseSchema(input, mod, error);
                    if (!schema) { REX::WARN("Settings provider {}: {}", mod, error); return Status::InvalidValue; }
                    if (!schema->hotkeys.empty() || !schema->menus.empty()) return Status::InvalidArgument;
                    const auto values = nlohmann::json::parse(valuesJson);
                    if (!values.is_object()) return Status::InvalidValue;
                    ModSettings record{ .schema = std::move(*schema) };
                    for (const auto& group : record.schema.groups) {
                        for (const auto& control : group.controls) {
                            const auto* setting = std::get_if<SettingDefinition>(&control);
                            if (!setting) return Status::InvalidArgument;
                            auto value = setting->DefaultValue();
                            if (const auto saved = values.find(setting->key); saved != values.end()) {
                                auto decoded = SettingsJson::DecodeValue(*saved, *setting);
                                if (!decoded || !IsValidValue(*setting, *decoded)) return Status::InvalidValue;
                                value = std::move(*decoded);
                            }
                            record.values.emplace(setting->key, std::move(value));
                        }
                    }
                    const auto result = SettingsService::Get().RegisterProvider(std::move(record),
                        [id = std::string(mod), save, context](const SettingValues& values) {
                            try {
                                const auto encoded = SettingsJson::EncodeValues(values).dump();
                                return save(id.c_str(), encoded.c_str(), context);
                            } catch (const std::exception& error) {
                                REX::WARN("Settings provider {}: {}", id, error.what());
                                return false;
                            }
                        }, *registration);
                    switch (result) {
                    case SettingsError::None: return Status::Ok;
                    case SettingsError::AlreadyRegistered: return Status::AlreadyRegistered;
                    case SettingsError::NotReady: return Status::NotReady;
                    default: return Status::InvalidArgument;
                    }
                } catch (const nlohmann::json::exception& error) {
                    REX::WARN("Settings provider {}: {}", mod, error.what());
                    return Status::InvalidValue;
                }
            }
            Status Unregister(Registration registration) noexcept override
            {
                return SettingsService::Get().UnregisterProvider(registration) == SettingsError::None ? Status::Ok : Status::InvalidArgument;
            }
            Status SubscribeKey(const char* mod, const char* key, HotkeyFn callback, void* context, Subscription* out) noexcept override
            {
                if (!mod || !IsValidModId(mod) || !key || !*key || !callback || !out) return Status::InvalidArgument;
                const auto token = KeyActions::Get().Subscribe(mod, key, [mod = std::string(mod), key = std::string(key), callback, context] {
                    callback(mod.c_str(), key.c_str(), context);
                });
                if (!token) return Status::InternalError;
                *out = token;
                return Status::Ok;
            }
            Status UnsubscribeKey(Subscription token) noexcept override
            {
                return KeyActions::Get().Unsubscribe(token) ? Status::Ok : Status::UnknownSubscription;
            }
        };
    }
    IProviders* GetProvidersApi() { static auto* api = new ProvidersApi; return api; }
}

extern "C" __declspec(dllexport) void* OSFSettings_RequestProvidersAPI(std::uint32_t version, std::uint32_t* outVersion) noexcept
{
    using namespace OSFSettings::API;
    if (outVersion) *outVersion = 0;
    if (!Supports(Providers::kVersion, version)) return nullptr;
    if (outVersion) *outVersion = Providers::kVersion;
    return Providers::GetProvidersApi();
}
