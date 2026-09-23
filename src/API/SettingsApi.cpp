#include "SettingsApi.h"
#include "../../sdk/OSFSettingsRegistry.h"
#include "Input/HotkeyInputState.h"
#include "Settings/SettingsService.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace OSFSettings::API
{
    namespace
    {
        Status ToStatus(SettingsError error) noexcept
        {
            switch (error) {
            case SettingsError::None: return Status::Ok;
            case SettingsError::NotReady: return Status::NotReady;
            case SettingsError::InvalidArgument: return Status::InvalidArgument;
            case SettingsError::UnknownMod: return Status::UnknownMod;
            case SettingsError::UnknownSetting: return Status::UnknownSetting;
            case SettingsError::TypeMismatch: return Status::TypeMismatch;
            case SettingsError::InvalidValue: return Status::InvalidValue;
            case SettingsError::SaveFailed: return Status::SaveFailed;
            case SettingsError::UnknownSubscription: return Status::UnknownSubscription;
            case SettingsError::InternalError: return Status::InternalError;
            case SettingsError::UnknownHotkey: return Status::UnknownHotkey;
            }
            return Status::InternalError;
        }

        std::uint32_t Count(std::size_t size)
        {
            if (size > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("registry field is too large");
            return static_cast<std::uint32_t>(size);
        }

        TextView Text(const std::string& text) { return { text.c_str(), Count(text.size()) }; }

        template <class T>
        const T* Elements(const std::vector<T>& items) { return items.empty() ? nullptr : items.data(); }

        template <class T>
        RegistryValue Value(const T& value)
        {
            if constexpr (std::is_same_v<T, bool>) return { .boolean = value };
            else if constexpr (std::is_same_v<T, std::int64_t>) return { .integer = value };
            else if constexpr (std::is_same_v<T, double>) return { .number = value };
            else if constexpr (std::is_same_v<T, EnumValue>) return { .text = Text(value.value) };
            else if constexpr (std::is_same_v<T, KeyBinding>) return { .key = value.keyCode };
            else return { .text = Text(value) };
        }

        SettingView Describe(const SettingDefinition& setting, const SettingValue& current, std::vector<EnumOptionView>& options)
        {
            SettingView view{ .key = Text(setting.key), .label = Text(setting.label), .hint = Text(setting.hint), .requiresRestart = setting.requiresRestart };
            std::visit([&](const auto& definition) {
                using Definition = std::decay_t<decltype(definition)>;
                using ValueType = std::decay_t<decltype(definition.defaultValue)>;
                view.value = Value(std::get<ValueType>(current));
                view.defaultValue = Value(definition.defaultValue);
                if constexpr (std::is_same_v<Definition, BoolDefinition>) {
                    view.type = SettingType::Bool;
                } else if constexpr (std::is_same_v<Definition, IntDefinition>) {
                    view.type = SettingType::Int;
                } else if constexpr (std::is_same_v<Definition, FloatDefinition>) {
                    view.type = SettingType::Float;
                    view.step = definition.step;
                } else if constexpr (std::is_same_v<Definition, EnumDefinition>) {
                    view.type = SettingType::Enum;
                    view.optionCount = Count(definition.options.size());
                    options.reserve(definition.options.size());
                    for (const auto& option : definition.options) options.push_back({ Text(option.value), Text(option.label) });
                    view.options = Elements(options);
                } else if constexpr (std::is_same_v<Definition, KeyDefinition>) {
                    view.type = SettingType::Key;
                    view.allowUnbound = definition.allowUnbound;
                } else if constexpr (std::is_same_v<Definition, StringDefinition>) {
                    view.type = SettingType::String;
                    view.maxLength = definition.maxLength;
                }
                if constexpr (std::is_same_v<Definition, IntDefinition> || std::is_same_v<Definition, FloatDefinition>) {
                    view.hasMinimum = definition.minimum.has_value();
                    view.hasMaximum = definition.maximum.has_value();
                    if (definition.minimum) view.minimum = Value(*definition.minimum);
                    if (definition.maximum) view.maximum = Value(*definition.maximum);
                }
            }, setting.definition);
            return view;
        }

        // Private backing arrays describe a detached service snapshot. None of these owning containers crosses the ABI or survives ReadRegistry.
        struct RegistryProjection
        {
            struct Group
            {
                std::vector<SettingView> settings;
                std::vector<std::vector<EnumOptionView>> options;
            };
            struct Mod
            {
                std::vector<Group> groups;
                std::vector<GroupView> views;
            };
            std::vector<Mod> mods;
            std::vector<ModView> views;

            explicit RegistryProjection(const std::vector<ModSettings>& snapshot, const char* selected)
            {
                // Size once so no later growth invalidates a published pointer.
                const auto count = selected ? 1u : Count(snapshot.size());
                mods.resize(count);
                views.reserve(count);
                for (const auto& mod : snapshot) {
                    if (selected && mod.schema.id != selected) continue;
                    auto& backing = mods[views.size()];
                    const auto& schema = mod.schema;
                    const auto groupCount = Count(schema.groups.size());
                    backing.groups.resize(groupCount);
                    backing.views.reserve(groupCount);
                    for (const auto& group : schema.groups) {
                        auto& data = backing.groups[backing.views.size()];
                        const auto settingCount = Count(std::ranges::count_if(group.controls, [](const auto& control) { return std::holds_alternative<SettingDefinition>(control); }));
                        data.settings.reserve(settingCount);
                        data.options.resize(settingCount);
                        for (const auto& control : group.controls) {
                            const auto* valueSetting = std::get_if<SettingDefinition>(&control);
                            if (!valueSetting) continue;
                            const auto& setting = *valueSetting;
                            data.settings.push_back(Describe(setting, mod.values.at(setting.key), data.options[data.settings.size()]));
                        }
                        backing.views.push_back({ Text(group.id), Text(group.label), Elements(data.settings), settingCount });
                    }
                    views.push_back({ Text(schema.id), Text(schema.title), Text(schema.description), Elements(backing.views), groupCount });
                }
            }
        };
    }

    SettingsApi::SettingsApi(SettingsService& service) : SettingsApi(service, HotkeyInputState::Get()) {}

    SettingsApi& SettingsApi::Get()
    {
        static auto* api = new SettingsApi(SettingsService::Get());
        return *api;
    }

    bool SettingsApi::IsReady() noexcept { return m_service.IsReady(); }

    Status SettingsApi::ReadRegistry(const char* mod, RegistryFn callback, void* context) noexcept
    {
        if (!callback || (mod && !IsValidModId(mod))) return Status::InvalidArgument;
        if (!m_service.IsReady()) return Status::NotReady;
        const auto snapshot = m_service.Snapshot();
        if (mod && std::ranges::none_of(snapshot, [&](const auto& entry) { return entry.schema.id == mod; })) {
            return Status::UnknownMod;
        }
        const RegistryProjection projection(snapshot, mod);
        const RegistryView view{ Elements(projection.views), Count(projection.views.size()) };
        callback(view, context);
        return Status::Ok;
    }

    template <class T>
    Status SettingsApi::Read(const char* mod, const char* key, T* out) noexcept
    {
        if (!mod || !key || !out) return Status::InvalidArgument;
        const auto result = m_service.GetValue(mod, key);
        if (!result) return ToStatus(result.error());
        const auto* value = std::get_if<T>(&*result);
        if (!value) return Status::TypeMismatch;
        *out = *value;
        return Status::Ok;
    }

    Status SettingsApi::GetBool(const char* mod, const char* key, bool* out) noexcept { return Read(mod, key, out); }
    Status SettingsApi::GetInt(const char* mod, const char* key, std::int64_t* out) noexcept { return Read(mod, key, out); }
    Status SettingsApi::GetFloat(const char* mod, const char* key, double* out) noexcept { return Read(mod, key, out); }

    Status SettingsApi::GetEnum(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept
    {
        return ReadText<EnumValue>(mod, key, out, capacity, required);
    }

    Status SettingsApi::GetString(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept
    {
        return ReadText<std::string>(mod, key, out, capacity, required);
    }

    template <class T>
    Status SettingsApi::ReadText(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept
    {
        if (!mod || !key || !required || (!out && capacity)) return Status::InvalidArgument;
        const auto result = m_service.GetValue(mod, key);
        if (!result) {
            return ToStatus(result.error());
        }
        const auto* value = std::get_if<T>(&*result);
        if (!value) {
            return Status::TypeMismatch;
        }
        const std::string* text;
        if constexpr (std::is_same_v<T, EnumValue>) {
            text = &value->value;
        } else {
            text = value;
        }
        if (text->size() >= std::numeric_limits<std::uint32_t>::max()) return Status::InternalError;
        *required = static_cast<std::uint32_t>(text->size() + 1);
        if (capacity < *required) return Status::BufferTooSmall;
        std::memcpy(out, text->c_str(), *required);
        return Status::Ok;
    }

    Status SettingsApi::GetKey(const char* mod, const char* key, std::uint32_t* out) noexcept
    {
        if (!out) return Status::InvalidArgument;
        KeyBinding binding;
        const auto status = Read(mod, key, &binding);
        if (status == Status::Ok) *out = binding.keyCode;
        return status;
    }

    Status SettingsApi::SetKey(const char* mod, const char* key, std::uint32_t value) noexcept
    {
        return Write(mod, key, KeyBinding{ value });
    }

    template <class T>
    Status SettingsApi::Write(const char* mod, const char* key, T value) noexcept
    {
        if (!mod || !key) return Status::InvalidArgument;
        return ToStatus(m_service.SetValue(mod, key, value));
    }

    Status SettingsApi::SetBool(const char* mod, const char* key, bool value) noexcept { return Write(mod, key, value); }
    Status SettingsApi::SetInt(const char* mod, const char* key, std::int64_t value) noexcept { return Write(mod, key, value); }
    Status SettingsApi::SetFloat(const char* mod, const char* key, double value) noexcept { return Write(mod, key, value); }
    Status SettingsApi::SetEnum(const char* mod, const char* key, const char* value) noexcept
    {
        if (!value) return Status::InvalidArgument;
        return Write(mod, key, EnumValue{ value });
    }

    Status SettingsApi::SetString(const char* mod, const char* key, const char* value, std::uint32_t length) noexcept
    {
        if (!mod || !key || !value) return Status::InvalidArgument;
        if (length > StringDefinition::MaxLength) return Status::InvalidValue;
        return Write(mod, key, std::string(value, length));
    }

    Status SettingsApi::Reset(const char* mod, const char* key) noexcept
    {
        if (!mod || !key) return Status::InvalidArgument;
        return ToStatus(m_service.Reset(mod, key));
    }

    Status SettingsApi::ResetMod(const char* mod) noexcept
    {
        if (!mod) return Status::InvalidArgument;
        return ToStatus(m_service.ResetMod(mod));
    }

    Status SettingsApi::Subscribe(const char* mod, ChangedFn callback, void* context, Subscription* out) noexcept
    {
        if (!mod || !callback || !out) return Status::InvalidArgument;
        return ToStatus(m_service.Subscribe(mod, [callback, context](const SettingsService::Change& change) noexcept {
            callback(change.mod.c_str(), change.key ? change.key->c_str() : nullptr, context);
        }, *out));
    }

    Status SettingsApi::Unsubscribe(Subscription subscription) noexcept
    {
        return ToStatus(m_service.Unsubscribe(subscription));
    }

    Status SettingsApi::RegisterHotkey(const char* mod, const char* id, HotkeyFn callback, void* context) noexcept
    {
        if (!mod || !id || !callback || !IsValidModId(mod)) return Status::InvalidArgument;
        const std::string_view hotkeyId(id);
        if (hotkeyId.empty() || hotkeyId.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != std::string_view::npos) return Status::InvalidArgument;
        if (!m_service.IsReady()) return Status::NotReady;
        return ToStatus(m_input.Register(mod, id, callback, context));
    }

    Status SettingsApi::AcquireHotkeyBlock(HotkeyBlock* out) noexcept
    {
        if (!out) return Status::InvalidArgument;
        const auto block = m_input.AcquireBlock();
        if (!block) return Status::InternalError;
        *out = block;
        return Status::Ok;
    }

    Status SettingsApi::ReleaseHotkeyBlock(HotkeyBlock block) noexcept
    {
        return m_input.ReleaseBlock(block) ? Status::Ok : Status::UnknownHotkeyBlock;
    }
}
