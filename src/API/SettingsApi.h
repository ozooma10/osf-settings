#pragma once

#include "../../sdk/OSFSettings.h"

namespace OSFSettings { class SettingsService; class HotkeyInputState; }

namespace OSFSettings::API
{
    class SettingsApi final : public ISettings
    {
    public:
        static SettingsApi& Get();
        explicit SettingsApi(SettingsService& service);
        SettingsApi(SettingsService& service, HotkeyInputState& input) : m_service(service), m_input(input) {}

        bool IsReady() noexcept override;
        Status GetBool(const char* mod, const char* key, bool* out) noexcept override;
        Status GetInt(const char* mod, const char* key, std::int64_t* out) noexcept override;
        Status GetFloat(const char* mod, const char* key, double* out) noexcept override;
        Status GetEnum(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept override;
        Status GetKey(const char* mod, const char* key, std::uint32_t* out) noexcept override;
        Status SetBool(const char* mod, const char* key, bool value) noexcept override;
        Status SetInt(const char* mod, const char* key, std::int64_t value) noexcept override;
        Status SetFloat(const char* mod, const char* key, double value) noexcept override;
        Status SetEnum(const char* mod, const char* key, const char* value) noexcept override;
        Status SetKey(const char* mod, const char* key, std::uint32_t value) noexcept override;
        Status Reset(const char* mod, const char* key) noexcept override;
        Status ResetMod(const char* mod) noexcept override;
        Status Subscribe(const char* mod, ChangedFn callback, void* context, Subscription* out) noexcept override;
        Status Unsubscribe(Subscription subscription) noexcept override;
        Status AcquireHotkeyBlock(HotkeyBlock* out) noexcept override;
        Status ReleaseHotkeyBlock(HotkeyBlock block) noexcept override;
        Status RegisterHotkey(const char* mod, const char* id, HotkeyFn callback, void* context) noexcept override;
        Status GetString(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept override;
        Status SetString(const char* mod, const char* key, const char* value, std::uint32_t length) noexcept override;
        Status ReadRegistry(const char* mod, RegistryFn callback, void* context) noexcept override;
        Status RegisterAction(const char* mod, const char* id, ActionFn callback, void* context) noexcept override;
        Status CompleteAction(Invocation invocation, bool succeeded, const char* message) noexcept override;
        Status GetLanguage(char* out, std::uint32_t capacity, std::uint32_t* required) noexcept override;

    private:
        template <class T> Status Read(const char* mod, const char* key, T* out) noexcept;
        template <class T> Status Write(const char* mod, const char* key, T value) noexcept;
        template <class T> Status ReadText(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept;
        SettingsService& m_service;
        HotkeyInputState& m_input;
    };
}
