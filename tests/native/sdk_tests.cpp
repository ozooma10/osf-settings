#include "../../sdk/OSFSettings.h"
#include <iostream>
#include <type_traits>

namespace
{
    int checks{}, failures{};
    void Check(bool passed, const char* message)
    {
        ++checks;
        if (!passed) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    }

    void TestSDK()
    {
        using namespace OSFSettings::API;
        static_assert(std::is_abstract_v<ISettings>);
        static_assert(!std::is_destructible_v<ISettings>);
        static_assert(Supports(0x00010001u, kBaseVersion));
        static_assert(!Supports(kBaseVersion, 0x00010001u));
        static_assert(!Supports(0x00020000u, kBaseVersion));

        struct Provider final : ISettings
        {
            bool ready{};
            std::string enumValue{ "quiet" };
            std::string nextEnumValue;
            Status enumCopyStatus{ Status::Ok };
            bool IsReady() noexcept override { return ready; }
            Status GetBool(const char*, const char*, bool* out) noexcept override
            {
                if (!ready) return Status::NotReady;
                *out = true;
                return Status::Ok;
            }
            Status GetInt(const char*, const char*, std::int64_t*) noexcept override { return Status::NotReady; }
            Status GetFloat(const char*, const char*, double*) noexcept override { return Status::NotReady; }
            Status GetEnum(const char*, const char*, char* out, std::uint32_t capacity, std::uint32_t* required) noexcept override
            {
                if (!ready) return Status::NotReady;
                if (out && enumCopyStatus != Status::Ok) return enumCopyStatus;
                *required = static_cast<std::uint32_t>(enumValue.size() + 1);
                if (!out) {
                    if (!nextEnumValue.empty()) {
                        enumValue.swap(nextEnumValue);
                        nextEnumValue.clear();
                    }
                    return Status::BufferTooSmall;
                }
                if (capacity < *required) return Status::BufferTooSmall;
                enumValue.copy(out, enumValue.size());
                out[enumValue.size()] = '\0';
                return Status::Ok;
            }
            Status SetBool(const char*, const char*, bool) noexcept override { return Status::SaveFailed; }
            Status SetInt(const char*, const char*, std::int64_t) noexcept override { return Status::NotReady; }
            Status SetFloat(const char*, const char*, double) noexcept override { return Status::NotReady; }
            Status SetEnum(const char*, const char*, const char*) noexcept override { return Status::NotReady; }
            Status Reset(const char*, const char*) noexcept override { return Status::NotReady; }
            Status ResetMod(const char*) noexcept override { return Status::NotReady; }
            Status Subscribe(const char*, ChangedFn, void*, Subscription*) noexcept override { return Status::NotReady; }
            Status Unsubscribe(Subscription) noexcept override { return Status::NotReady; }
            Status GetKey(const char*, const char*, std::uint32_t*) noexcept override { return Status::NotReady; }
            Status SetKey(const char*, const char*, std::uint32_t) noexcept override { return Status::NotReady; }
            Status RegisterHotkey(const char*, const char*, HotkeyFn, void*) noexcept override { return Status::NotReady; }
            Status AcquireHotkeyBlock(HotkeyBlock*) noexcept override { return Status::NotReady; }
            Status ReleaseHotkeyBlock(HotkeyBlock) noexcept override { return Status::NotReady; }
            Status GetString(const char*, const char*, char*, std::uint32_t, std::uint32_t*) noexcept override { return Status::NotReady; }
            Status SetString(const char*, const char*, const char*, std::uint32_t) noexcept override { return Status::NotReady; }
            Status ReadRegistry(const char*, RegistryFn, void*) noexcept override { return Status::NotReady; }
            Status RegisterAction(const char*, const char*, ActionFn, void*) noexcept override { return Status::NotReady; }
            Status CompleteAction(Invocation, bool, const char*) noexcept override { return Status::NotReady; }
        } provider;

        Client client;
        bool enabled = false;
        std::uint32_t required = 42;
        Subscription subscription = 17;
        char text[] = "unchanged";
        std::string mode = "unchanged";
        Check(!client && !client.IsReady() && !client.Raw() && client.Version() == 0 && !client.Has(kBaseVersion),
            "a new SDK client has no service");
        Check(client.GetBool("learning", "notifications", &enabled) == Status::NotReady && !enabled &&
            client.GetEnum("learning", "mode", text, sizeof(text), &required) == Status::NotReady &&
            std::string_view(text) == "unchanged" && required == 42 &&
            client.Subscribe("learning", nullptr, nullptr, &subscription) == Status::NotReady && subscription == 17,
            "detached SDK reads and subscriptions preserve caller outputs");
        Check(client.GetEnum("learning", "mode", mode) == Status::NotReady && mode == "unchanged",
            "detached SDK string reads preserve the caller's string");

        Check(client.Attach(&provider, 0x00010001u) && client && client.Raw() == &provider &&
            client.Version() == 0x00010001u && client.Has(kBaseVersion) && !client.Has(0x00010002u) && !client.IsReady(),
            "SDK attachment accepts a newer compatible minor independently of readiness");
        Check(client.GetBool("learning", "notifications", &enabled) == Status::NotReady && !enabled,
            "an attached SDK client preserves provider readiness failures");
        Check(client.GetEnum("learning", "mode", mode) == Status::NotReady && mode == "unchanged",
            "SDK string reads preserve output when the provider is not ready");
        provider.ready = true;
        Check(client.IsReady() && client.GetBool("learning", "notifications", &enabled) == Status::Ok && enabled,
            "an SDK client observes provider readiness and current values");
        Check(client.SetBool("learning", "notifications", false) == Status::SaveFailed,
            "SDK writes preserve a provider save failure");
        Check(client.GetEnum("learning", "mode", mode) == Status::Ok && mode == "quiet",
            "SDK string reads exclude the terminating NUL");
        provider.nextEnumValue = std::string(80, 'x');
        Check(client.GetEnum("learning", "mode", mode) == Status::Ok && mode == std::string(80, 'x'),
            "SDK string reads retry when the value grows after the size query");
        provider.nextEnumValue = "quiet";
        Check(client.GetEnum("learning", "mode", mode) == Status::Ok && mode == "quiet",
            "SDK string reads use the actual length when the value shrinks after the size query");
        provider.enumCopyStatus = Status::UnknownSetting;
        Check(client.GetEnum("learning", "mode", mode) == Status::UnknownSetting && mode == "quiet",
            "SDK string reads preserve output when copying fails after a successful size query");
        provider.enumCopyStatus = Status::Ok;

        Check(!client.Attach(&provider, 0x00020000u) && !client && !client.Raw() && client.Version() == 0,
            "incompatible SDK attachment clears an existing service");
        client.Attach(&provider);
        Check(!client.Attach(nullptr) && !client && client.Version() == 0,
            "null SDK attachment clears an existing service");

        std::uint32_t actual = 42;
        Check(RequestInterface(kBaseVersion, &actual) == nullptr && actual == 0,
            "SDK discovery reports a missing provider without loading it");
        client.Attach(&provider);
        Check(!client.Init() && !client && !client.Raw() && client.Version() == 0,
            "failed SDK discovery clears an existing attachment");
    }

}

int main()
{
    TestSDK();
    std::cout << checks - failures << '/' << checks << " SDK checks passed\n";
    return failures ? 1 : 0;
}
