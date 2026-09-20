#pragma once

#include "Settings/SettingsService.h"
#include "REX/LOG.h"

#include <cmath>
#include <limits>
#include <type_traits>

namespace OSFSettings::Papyrus
{
    const char* ErrorName(SettingsError error) noexcept;
    bool Report(std::string_view function, std::string_view mod, std::string_view key, SettingsError error);

    // Check narrowing before CommonLib's native return-value marshaler runs.
    template <class T>
    std::expected<T, SettingsError> Convert(const SettingValue& value)
    {
        using Stored = std::conditional_t<std::is_same_v<T, std::int32_t>, std::int64_t, std::conditional_t<std::is_same_v<T, float>, double, T>>;
        const auto* stored = std::get_if<Stored>(&value);
        if (!stored) return std::unexpected(SettingsError::TypeMismatch);
        if constexpr (std::is_same_v<T, std::int32_t>) {
            if (*stored < INT32_MIN || *stored > INT32_MAX) {
                return std::unexpected(SettingsError::InvalidValue);
            }
        } else if constexpr (std::is_same_v<T, float>) {
            if (!std::isfinite(*stored) || std::abs(*stored) > std::numeric_limits<float>::max()) {
                return std::unexpected(SettingsError::InvalidValue);
            }
        }
        return static_cast<T>(*stored);
    }

    class Values
    {
    public:
        explicit Values(SettingsService& service) : m_service(service) {}

        template <class T>
        T Read(std::string_view function, std::string_view mod, std::string_view key, T fallback) const
        {
            const auto value = m_service.GetValue(mod, key);
            const auto result = value ? Convert<T>(*value) : std::expected<T, SettingsError>(std::unexpected(value.error()));
            if (result) return *result;
            Report(function, mod, key, result.error());
            return fallback;
        }

        bool Write(std::string_view function, std::string_view mod, std::string_view key, SettingValue value)
        {
            return Report(function, mod, key, m_service.SetValue(mod, key, std::move(value)));
        }
        bool Reset(std::string_view mod, std::string_view key)
        {
            return Report("Reset", mod, key, m_service.Reset(mod, key));
        }
        bool ResetMod(std::string_view mod)
        {
            return Report("ResetMod", mod, {}, m_service.ResetMod(mod));
        }

    private:
        SettingsService& m_service;
    };
}
