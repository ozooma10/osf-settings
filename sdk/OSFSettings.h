// OSF Settings native service.
#pragma once

#include <cstdint>
#include <string>
#include "REX/W32/KERNEL32.h"

namespace OSFSettings::API
{
    // Packed major.minor service versions.
    inline constexpr std::uint32_t kVersion = 0x00010000u;
    inline constexpr std::uint32_t kBaseVersion = 0x00010000u;
    inline constexpr wchar_t kModuleName[] = L"OSF Settings Slim.dll";
    inline constexpr char kRequestExportName[] = "OSFSettings_RequestAPI";

    constexpr bool Supports(std::uint32_t have, std::uint32_t need) noexcept
    {
        return (have >> 16) == (need >> 16) && (have & 0xFFFFu) >= (need & 0xFFFFu);
    }

    enum class Status : std::uint32_t
    {
        Ok = 0,
        NotReady = 1,
        InvalidArgument = 2,
        UnknownMod = 3,
        UnknownSetting = 4,
        TypeMismatch = 5,
        InvalidValue = 6,
        BufferTooSmall = 7,
        SaveFailed = 8,
        UnknownSubscription = 9,
        InternalError = 10
    };

    using Subscription = std::uint64_t; // Zero is never a valid subscription.

    // Reread current settings. key == nullptr requests a full refresh, including the initial notification.
    // String pointers are borrowed only for this callback; user belongs to the caller.
    // Mods handle their own exceptions; none may escape the callback.
    // Callbacks run serially from an SFSE task; this API does not guarantee the main thread. schedule thread-sensitive game work appropriately.
    using ChangedFn = void (*)(const char* mod, const char* key, void* user) noexcept;

    struct ISettings
    {
        virtual bool IsReady() noexcept = 0;

        // Exact setting types. On failure, scalar outputs remain unchanged.
        virtual Status GetBool(const char* mod, const char* key, bool* out) noexcept = 0;
        virtual Status GetInt(const char* mod, const char* key, std::int64_t* out) noexcept = 0;
        virtual Status GetFloat(const char* mod, const char* key, double* out) noexcept = 0;

        // Caller-owned UTF-8 buffer. required includes the final NUL.
        // nullptr/0 queries the size and returns BufferTooSmall. Never truncates.
        virtual Status GetEnum(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept = 0;

        // Ok means saved and published, or already equal.
        virtual Status SetBool(const char* mod, const char* key, bool value) noexcept = 0;
        virtual Status SetInt(const char* mod, const char* key, std::int64_t value) noexcept = 0;
        virtual Status SetFloat(const char* mod, const char* key, double value) noexcept = 0;
        virtual Status SetEnum(const char* mod, const char* key, const char* value) noexcept = 0;
        virtual Status Reset(const char* mod, const char* key) noexcept = 0;
        virtual Status ResetMod(const char* mod) noexcept = 0;

        // Subscribe before reading to avoid missing changes. Registration is allowed before readiness; the initial notification waits until the provider is ready.
        virtual Status Subscribe(const char* mod, ChangedFn callback, void* user, Subscription* out) noexcept = 0;
        // Outside a callback, successful unsubscribe waits for that callback to finish.
        virtual Status Unsubscribe(Subscription subscription) noexcept = 0;

    protected:
        ~ISettings() = default;
    };

    using AcquireFn = void* (*)(std::uint32_t version, std::uint32_t* outVersion) noexcept;

    inline ISettings* RequestInterface(std::uint32_t version = kBaseVersion, std::uint32_t* outVersion = nullptr) noexcept
    {
        if (outVersion) *outVersion = 0;
        const auto module = REX::W32::GetModuleHandleW(kModuleName);
        if (!module) return nullptr;
        const auto fn = reinterpret_cast<AcquireFn>(REX::W32::GetProcAddress(module, kRequestExportName));
        return fn ? static_cast<ISettings*>(fn(version, outVersion)) : nullptr;
    }

    class Client
    {
    public:
        // Acquires and caches the service after SFSE kPostPostLoad.
        bool Init(std::uint32_t version = kBaseVersion) noexcept
        {
            std::uint32_t actual{};
            auto* api = RequestInterface(version, &actual);
            return Attach(api, actual);
        }

        // Borrows the interface; nullptr or an incompatible version detaches.
        bool Attach(ISettings* api, std::uint32_t version = kVersion) noexcept
        {
            m_api = api && Supports(version, kBaseVersion) ? api : nullptr;
            m_version = m_api ? version : 0;
            return m_api != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept { return m_api != nullptr; }
        [[nodiscard]] std::uint32_t Version() const noexcept { return m_version; }
        [[nodiscard]] bool Has(std::uint32_t version) const noexcept { return m_api && Supports(m_version, version); }
        [[nodiscard]] ISettings* Raw() const noexcept { return m_api; }
        [[nodiscard]] bool IsReady() const noexcept { return m_api && m_api->IsReady(); }

        Status GetBool(const char* mod, const char* key, bool* out) const noexcept
        {
            return m_api ? m_api->GetBool(mod, key, out) : Status::NotReady;
        }
        Status GetInt(const char* mod, const char* key, std::int64_t* out) const noexcept
        {
            return m_api ? m_api->GetInt(mod, key, out) : Status::NotReady;
        }
        Status GetFloat(const char* mod, const char* key, double* out) const noexcept
        {
            return m_api ? m_api->GetFloat(mod, key, out) : Status::NotReady;
        }
        Status GetEnum(const char* mod, const char* key, char* out, std::uint32_t capacity, std::uint32_t* required) const noexcept
        {
            return m_api ? m_api->GetEnum(mod, key, out, capacity, required) : Status::NotReady;
        }
        Status GetEnum(const char* mod, const char* key, std::string& out) const noexcept
        {
            std::string buffer;
            std::uint32_t required{};
            auto status = GetEnum(mod, key, nullptr, 0, &required);
            while (status == Status::BufferTooSmall) {
                buffer.resize(required);
                status = GetEnum(mod, key, buffer.data(), static_cast<std::uint32_t>(buffer.size()), &required);
            }
            if (status == Status::Ok) {
                buffer.resize(required - 1); // Exclude the terminating NUL.
                out.swap(buffer);
            }
            return status;
        }

        Status SetBool(const char* mod, const char* key, bool value) const noexcept
        {
            return m_api ? m_api->SetBool(mod, key, value) : Status::NotReady;
        }
        Status SetInt(const char* mod, const char* key, std::int64_t value) const noexcept
        {
            return m_api ? m_api->SetInt(mod, key, value) : Status::NotReady;
        }
        Status SetFloat(const char* mod, const char* key, double value) const noexcept
        {
            return m_api ? m_api->SetFloat(mod, key, value) : Status::NotReady;
        }
        Status SetEnum(const char* mod, const char* key, const char* value) const noexcept
        {
            return m_api ? m_api->SetEnum(mod, key, value) : Status::NotReady;
        }
        Status Reset(const char* mod, const char* key) const noexcept
        {
            return m_api ? m_api->Reset(mod, key) : Status::NotReady;
        }
        Status ResetMod(const char* mod) const noexcept
        {
            return m_api ? m_api->ResetMod(mod) : Status::NotReady;
        }

        Status Subscribe(const char* mod, ChangedFn callback, void* user, Subscription* out) const noexcept
        {
            return m_api ? m_api->Subscribe(mod, callback, user, out) : Status::NotReady;
        }
        Status Unsubscribe(Subscription subscription) const noexcept
        {
            return m_api ? m_api->Unsubscribe(subscription) : Status::NotReady;
        }

    private:
        ISettings* m_api{};
        std::uint32_t m_version{};
    };
}
