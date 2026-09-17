#include "SettingsApi.h"
#include "Input/HotkeyInputState.h"
#include "Settings/SettingsService.h"

#include <cstring>
#include <limits>

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
            }
            return Status::InternalError;
        }
    }

    SettingsApi::SettingsApi(SettingsService& service) : SettingsApi(service, HotkeyInputState::Get()) {}

    SettingsApi& SettingsApi::Get()
    {
        static auto* api = new SettingsApi(SettingsService::Get());
        return *api;
    }

    bool SettingsApi::IsReady() noexcept { return m_service.IsReady(); }

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
        if (!mod || !key || !required || (!out && capacity)) return Status::InvalidArgument;
        const auto result = m_service.GetValue(mod, key);
        if (!result) return ToStatus(result.error());
        const auto* text = std::get_if<std::string>(&*result);
        if (!text) return Status::TypeMismatch;
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
        return Write(mod, key, value);
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

    Status SettingsApi::Subscribe(const char* mod, ChangedFn callback, void* user, Subscription* out) noexcept
    {
        if (!mod || !callback || !out) return Status::InvalidArgument;
        return ToStatus(m_service.Subscribe(mod, [callback, user](const SettingsService::Change& change) noexcept {
            callback(change.mod.c_str(), change.key ? change.key->c_str() : nullptr, user);
        }, *out));
    }

    Status SettingsApi::Unsubscribe(Subscription subscription) noexcept
    {
        return ToStatus(m_service.Unsubscribe(subscription));
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
