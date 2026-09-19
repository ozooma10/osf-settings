#pragma once

#include "OSFSettingsRegistry.h"

#include <atomic>
#include <map>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>

namespace RegistryExample
{
    namespace API = OSFSettings::API;

    // These are consumer-owned types, never passed across the plugin boundary.
    struct Setting
    {
        API::SettingType type;
        std::variant<bool, std::int64_t, double, std::string, std::uint32_t> value;
    };
    using ModValues = std::map<std::string, Setting, std::less<>>;
    using Values = std::map<std::string, ModValues, std::less<>>;

    inline std::string Copy(API::TextView text) { return { text.data, text.size }; }

    class RegistryConsumer
    {
    public:
        explicit RegistryConsumer(API::Client settings) : m_settings(settings) {}
        ~RegistryConsumer() { Stop(); }

        // Call Start/Stop serially from the owner, outside provider callbacks.
        // Initialize the client before construction and keep it attached.
        API::Status Start() noexcept
        {
            if (m_started) return API::Status::InvalidArgument;
            try {
                Capture discovery;
                auto status = Read(nullptr, discovery);
                if (status != API::Status::Ok) return status;
                m_subscriptions.reserve(discovery.values.size());
                for (const auto& [mod, values] : discovery.values) {
                    API::Subscription token{};
                    status = m_settings.Subscribe(mod.c_str(), Changed, this, &token);
                    if (status != API::Status::Ok) { Stop(); return status; }
                    m_subscriptions.push_back(token);
                }
                // Discovery values were captured before subscription: do not
                // publish them. Read again after every subscription is installed.
                status = Refresh(nullptr);
                if (status != API::Status::Ok) { Stop(); return status; }
                m_started = true;
                return status;
            } catch (...) {
                Stop();
                return API::Status::InternalError;
            }
        }

        void Stop() noexcept
        {
            // Never hold m_mutex here: Unsubscribe may wait for Changed, which
            // needs that mutex. The owner remains alive until every call returns.
            for (auto token : m_subscriptions) m_settings.Unsubscribe(token);
            m_subscriptions.clear();
            m_started = false;
        }

        Values Snapshot() const
        {
            std::lock_guard lock(m_mutex);
            return m_values;
        }

        API::Status LastRefresh() const noexcept { return m_lastRefresh.load(); }

    private:
        struct Capture
        {
            Values values;
            bool copied{};

            static void OnRegistry(const API::RegistryView& registry, void* context) noexcept
            {
                auto& self = *static_cast<Capture*>(context);
                try {
                    for (std::uint32_t m = 0; m < registry.modCount; ++m) {
                        const auto& mod = registry.mods[m];
                        auto& values = self.values[Copy(mod.id)]; // Include empty mods.
                        for (std::uint32_t g = 0; g < mod.groupCount; ++g) {
                            const auto& group = mod.groups[g];
                            for (std::uint32_t s = 0; s < group.settingCount; ++s) {
                                const auto& setting = group.settings[s];
                                Setting owned{ setting.type, false };
                                switch (setting.type) {
                                case API::SettingType::Bool: owned.value = setting.value.boolean; break;
                                case API::SettingType::Int: owned.value = setting.value.integer; break;
                                case API::SettingType::Float: owned.value = setting.value.number; break;
                                case API::SettingType::Enum:
                                case API::SettingType::String: owned.value = Copy(setting.value.text); break;
                                case API::SettingType::Key: owned.value = setting.value.key; break;
                                default: return; // Cannot publish an incomplete capture.
                                }
                                values.emplace(Copy(setting.key), std::move(owned));
                            }
                        }
                    }
                    self.copied = true;
                } catch (...) {
                    // A noexcept callback must handle its own allocation errors.
                    // The provider's Ok only reports successful snapshot delivery.
                }
            }
        };

        API::Status Read(const char* mod, Capture& capture) const noexcept
        {
            const auto status = m_settings.ReadRegistry(mod, Capture::OnRegistry, &capture);
            return status == API::Status::Ok && !capture.copied ? API::Status::InternalError : status;
        }

        API::Status Refresh(const char* mod) noexcept
        {
            try {
                // Serialize acquisition AND replacement. Locking only the final
                // assignment could let an older capture replace a newer one.
                std::lock_guard lock(m_mutex);
                Capture capture;
                const auto status = Read(mod, capture);
                if (status != API::Status::Ok) return status;
                if (mod) m_values.insert_or_assign(mod, std::move(capture.values.begin()->second));
                else m_values.swap(capture.values);
                return API::Status::Ok;
            } catch (...) {
                return API::Status::InternalError;
            }
        }

        static void Changed(const char* mod, const char* key, void* context) noexcept
        {
            auto& self = *static_cast<RegistryConsumer*>(context);
            if (key == nullptr) {
                // Initial notification or whole-mod reset: reread every value.
                self.m_lastRefresh.store(self.Refresh(mod));
                return;
            }
            // A keyed invalidation also refreshes the mod in this small example.
            // The key is never stored or treated as a value-bearing event.
            self.m_lastRefresh.store(self.Refresh(mod));
        }

        API::Client m_settings;
        std::vector<API::Subscription> m_subscriptions;
        mutable std::mutex m_mutex;
        Values m_values;
        std::atomic<API::Status> m_lastRefresh{ API::Status::NotReady };
        bool m_started{};
    };
}
