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
            const auto resolved = Resolve(mod, key);
            const auto value = resolved ? m_service.GetValue(resolved->first, resolved->second) : std::expected<SettingValue, SettingsError>(std::unexpected(resolved.error()));
            const auto result = value ? Convert<T>(*value) : std::expected<T, SettingsError>(std::unexpected(value.error()));
            if (result) return *result;
            Report(function, mod, key, result.error());
            return fallback;
        }

        bool Write(std::string_view function, std::string_view mod, std::string_view key, SettingValue value)
        {
            const auto resolved = Resolve(mod, key, &value);
            return Report(function, mod, key, resolved ? m_service.SetValue(resolved->first, resolved->second, std::move(value)) : resolved.error());
        }
        bool Reset(std::string_view mod, std::string_view key)
        {
            const auto resolved = Resolve(mod, key);
            return Report("Reset", mod, key, resolved ? m_service.Reset(resolved->first, resolved->second) : resolved.error());
        }
        bool ResetMod(std::string_view mod)
        {
            return Report("ResetMod", mod, {}, m_service.ResetMod(FoldAscii(mod)));
        }

    private:
        std::expected<std::pair<std::string, std::string>, SettingsError> Resolve(std::string_view mod, std::string_view key, SettingValue* value = nullptr) const;
        SettingsService& m_service;
    };
}
