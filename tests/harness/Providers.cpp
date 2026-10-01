#include "Acceptance.h"
#include "../../sdk/OSFSettings_Providers.h"
#include "Persistence/AtomicFile.h"
#include "Utils/Paths.h"
#include <fstream>
#include <mutex>

namespace OSFSettings::TestHarness
{
    namespace
    {
        using Json = nlohmann::json;
        constexpr auto Mod = "osfacceptance-provider";
        constexpr auto Schema = R"({"title":"Provider Acceptance","groups":{"General":[
            {"key":"enabled","label":"Provider enabled","type":"bool","default":false},
            {"key":"binding","label":"Observed key","type":"key","default":255,"allowMouse":true}
        ]}})";
        std::mutex mutex;
        API::Providers::Registration registration{};
        API::Subscription observer{}, changes{};
        bool rejectSave{};
        unsigned writes{}, presses{}, notifications{};
        Json persisted = Json::object();

        void Require(API::Status status)
        {
            if (status != API::Status::Ok) throw std::runtime_error("provider fixture status " + std::to_string(static_cast<unsigned>(status)));
        }
        std::filesystem::path Store() { return Paths::UserDataDir() / "provider-consumer.json"; }
        bool Save(const char*, const char* values, void*) noexcept
        {
            try {
                std::scoped_lock lock(mutex);
                ++writes;
                if (rejectSave) return false;
                const auto parsed = Json::parse(values);
                std::string error;
                if (!Persistence::WriteAtomic(Store(), parsed.dump(), error)) return false;
                persisted = parsed;
                return true;
            } catch (...) { return false; }
        }
    }

    void ProviderCommand(const Json& args)
    {
        API::Providers::Client providers;
        API::Client settings;
        if (!providers.Init() || !settings.Init()) throw std::runtime_error("provider API unavailable");
        const auto operation = args.at("operation").get<std::string>();
        if (operation == "providerRegister") {
            auto initial = Json::object();
            { std::ifstream input(Store()); if (input) initial = Json::parse(input); }
            // Subscribe before registration, as a real caller may do at PostLoad.
            if (!observer) Require(providers.SubscribeKey(Mod, "binding", +[](const char*, const char*, void*) noexcept {
                std::scoped_lock lock(mutex); ++presses;
            }, nullptr, &observer));
            if (!changes) Require(settings.Subscribe(Mod, +[](const char*, const char*, void*) noexcept {
                std::scoped_lock lock(mutex); ++notifications;
            }, nullptr, &changes));
            Require(providers.Register(Mod, Schema, initial.dump().c_str(), Save, nullptr, &registration));
            { std::scoped_lock lock(mutex); persisted = initial; }
        } else if (operation == "providerReject") {
            std::scoped_lock lock(mutex); rejectSave = args.at("value").get<bool>();
        } else if (operation == "providerKey") {
            Require(settings.SetKey(Mod, "binding", args.at("value").get<std::uint32_t>()));
        } else if (operation == "providerUnsubscribe") {
            Require(providers.UnsubscribeKey(observer)); observer = 0;
        } else if (operation == "providerUnregister") {
            Require(providers.Unregister(registration)); registration = 0;
        } else if (operation == "providerReset") {
            Require(settings.ResetMod(Mod));
        } else throw std::runtime_error("unknown-provider-operation");
    }

    Json ProviderSnapshot()
    {
        API::Client settings;
        bool enabled{}; std::uint32_t key{};
        const bool present = settings.Init() && settings.GetBool(Mod, "enabled", &enabled) == API::Status::Ok;
        if (present) settings.GetKey(Mod, "binding", &key);
        std::scoped_lock lock(mutex);
        return {{"present", present}, {"enabled", enabled}, {"key", key}, {"writes", writes},
            {"presses", presses}, {"notifications", notifications}, {"persisted", persisted}};
    }
}
