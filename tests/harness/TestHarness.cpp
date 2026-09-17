#include "TestHarness.h"

#include "Settings/SettingsService.h"
#include "Utils/Paths.h"
#include "RE/B/BSService.h"
#include "RE/C/ControlMap.h"
#include <Windows.h>
#undef ERROR
#include <nlohmann/json.hpp>
#include <atomic>
#include <cstring>
#include <mutex>

namespace OSFSettings::TestHarness
{
    namespace
    {
        using Json = nlohmann::json;
        using Value = RE::Scaleform::GFx::Value;
        std::mutex g_mutex;
        Json g_state = {
            {"inputAttached", false}, {"menuOpen", false}, {"openKeyCode", nullptr},
            {"bindingObservedAtMs", 0}, {"hotkeyInput", {{"sequence", 0}, {"pressSequence", 0}}},
            {"menuInput", {{"sequence", 0}, {"pressSequence", 0}}}, {"ui", Json::object()}
        };
        std::atomic_bool g_queued{};
        std::atomic<std::uint64_t> g_lastPoll{};

        Json CopyValue(const Value& value, unsigned depth = 0)
        {
            if (depth > 6) return nullptr;
            if (value.IsBoolean()) return value.GetBoolean();
            if (value.IsInt()) return value.GetInt();
            if (value.IsUInt()) return value.GetUInt();
            if (value.IsNumber()) return value.GetNumber();
            if (value.IsString()) return value.GetString();
            if (value.IsArray()) {
                auto result = Json::array();
                class Collector final : public Value::ArrayVisitor
                {
                public:
                    Collector(Json& result, unsigned depth) : result(result), depth(depth) {}
                    void Visit(std::uint32_t index, const Value& entry) override
                    {
                        try { if (index < 128) result.push_back(CopyValue(entry, depth + 1)); } catch (...) {}
                    }
                    Json& result;
                    unsigned depth;
                } collector(result, depth);
                auto array = value;
                array.VisitElements(&collector);
                return result;
            }
            if (value.IsObject()) {
                auto result = Json::object();
                // Deliberately only the fields this development movie emits.
                for (const auto* field : {"initialized", "closing", "refreshing", "mod", "group", "selectedIndex",
                    "selection", "rows", "stage", "capture", "kind", "index", "key", "type", "value", "title",
                    "rect", "controlRect", "x", "y", "width", "height", "visibleRect", "active", "ready",
                    "editable", "minimum", "maximum", "status", "scrollPosition", "startupPhase", "state", "diagnosticError",
                    "mouse", "mouseDown", "mouseClick", "sequence", "frame", "target", "buttonDown", "saving"}) {
                    Value member;
                    if (value.GetMember(field, &member) && !member.IsUndefined()) result[field] = CopyValue(member, depth + 1);
                }
                return result;
            }
            return nullptr;
        }

        void ObserveBinding() noexcept
        {
            // Queue submission can execute inline during startup. Refuse that lane.
            if (RE::BSService::TaskQueue::GetDrainOwnerThreadID() != ::GetCurrentThreadId() ||
                !RE::BSService::TaskQueue::IsQueueEnabled()) return;
            try {
                Json binding = nullptr;
                std::uint32_t testBinding = 255;
                std::uint32_t freeTestKey = 255;
                auto menuBindings = Json::object();
                if (const auto* map = RE::ControlMap::GetSingleton()) {
                    bool occupied[256]{};
                    for (const auto& entry : map->GetMappings(RE::ControlMap::InputContextID::kMainGameplay,
                        RE::InputEvent::DeviceType::kKeyboard)) {
                        if (entry.keyCode < 256) occupied[entry.keyCode] = true;
                        if (std::string_view(entry.eventID.c_str()) == "osfsettings/openMenu" &&
                            entry.bindingSlot == RE::ControlMap::BindingSlot::kMain) {
                            binding = entry.keyCode;
                        }
                        if (std::string_view(entry.eventID.c_str()) == "learning/testHotkey" &&
                            entry.bindingSlot == RE::ControlMap::BindingSlot::kMain) {
                            testBinding = entry.keyCode;
                        }
                    }
                    for (const std::uint32_t key : { 117u, 118u, 119u, 122u }) {
                        if (!occupied[key]) { freeTestKey = key; break; }
                    }
                    // Settings requests this native navigation context. Preserve
                    // slots/chords instead of pretending an alternate is the main key.
                    for (const auto& entry : map->GetMappings(RE::ControlMap::InputContextID::kBasicMenuNav,
                        RE::InputEvent::DeviceType::kKeyboard)) {
                        const auto action = std::string(entry.eventID.c_str());
                        if (!menuBindings.contains(action)) menuBindings[action] = Json::array();
                        menuBindings[action].push_back({{"keyCode", entry.keyCode},
                            {"modifierKeyCode", entry.modifierKeyCode}, {"slot", static_cast<std::uint32_t>(entry.bindingSlot)},
                            {"contextID", static_cast<std::uint32_t>(RE::ControlMap::InputContextID::kBasicMenuNav)}});
                    }
                }
                std::scoped_lock lock(g_mutex);
                g_state["openKeyCode"] = std::move(binding);
                g_state["testHotkeyCode"] = testBinding;
                g_state["freeTestKey"] = freeTestKey;
                g_state["menuBindings"] = std::move(menuBindings);
                g_state["menuBindingsObservedAtMs"] = ::GetTickCount64();
                g_state["bindingObservedAtMs"] = ::GetTickCount64();
            } catch (...) {}
        }
    }

    void Poll() noexcept
    {
        const auto now = ::GetTickCount64();
        if (now - g_lastPoll.load() < 250 || g_queued.exchange(true)) return;
        g_lastPoll.store(now);
        try {
            if (auto* queue = RE::BSService::TaskQueue::GetSingleton(); queue && RE::BSService::TaskQueue::IsQueueEnabled()) {
                queue->AddTask([] { ObserveBinding(); g_queued.store(false); });
            } else g_queued.store(false);
        } catch (...) { g_queued.store(false); }
    }

    void SetInputAttached(bool attached) noexcept
    {
        try { std::scoped_lock lock(g_mutex); g_state["inputAttached"] = attached; } catch (...) {}
    }

    void MenuState(bool open) noexcept
    {
        try {
            std::scoped_lock lock(g_mutex);
            g_state["menuOpen"] = open;
            g_state["menuObservedAtMs"] = ::GetTickCount64();
            if (!open) g_state["ui"] = Json::object();
        } catch (...) {}
    }

    void ObserveInput(const RE::ButtonEvent* event, bool hotkey) noexcept
    {
        if (!event) return;
        try {
            std::scoped_lock lock(g_mutex);
            auto& input = g_state[hotkey ? "hotkeyInput" : "menuInput"];
            const auto sequence = input.value("sequence", std::uint64_t{}) + 1;
            const auto presses = input.value("pressSequence", std::uint64_t{}) +
                (event->value > 0 && event->heldDownSecs == 0 ? 1 : 0);
            input = {{"sequence", sequence}, {"pressSequence", presses}, {"observedAtMs", ::GetTickCount64()},
                {"device", static_cast<std::uint32_t>(event->deviceType)}, {"key", event->idCode},
                {"userEvent", event->QUserEvent().c_str()}, {"value", event->value}, {"held", event->heldDownSecs}};
        } catch (...) {}
    }

    void ObserveUI(std::uint64_t frame, const Value* state) noexcept
    {
        try {
            Json copy;
            if (state && state->IsObject()) copy = CopyValue(*state);
            std::scoped_lock lock(g_mutex);
            if (copy.is_object()) {
                g_state["ui"] = std::move(copy);
                g_state["ui"]["layoutObservedAtMs"] = ::GetTickCount64();
            }
            g_state["ui"]["frame"] = frame;
            g_state["ui"]["observedAtMs"] = ::GetTickCount64();
        } catch (...) {}
    }

    std::uint32_t Snapshot(char* buffer, std::uint32_t capacity) noexcept
    {
        if (buffer && capacity) buffer[0] = '\0';
        try {
            Json result;
            { std::scoped_lock lock(g_mutex); result = g_state; }
            auto& service = SettingsService::Get();
            result["ready"] = service.IsReady();
            result["valuesDirectory"] = Paths::ValuesDir().string();
            result["sampledAtMs"] = ::GetTickCount64();
            auto values = Json::object();
            for (const auto* key : {"notifications", "notificationLimit", "notificationVolume", "notificationMode", "notificationKey"}) {
                if (const auto value = service.GetValue("learning", key)) {
                    std::visit([&](const auto& current) {
                        if constexpr (std::is_same_v<std::decay_t<decltype(current)>, KeyBinding>) values[key] = current.keyCode;
                        else values[key] = current;
                    }, *value);
                }
            }
            result["learning"] = std::move(values);
            const auto text = result.dump();
            const auto required = static_cast<std::uint32_t>(text.size() + 1);
            if (buffer && capacity >= required) std::memcpy(buffer, text.c_str(), required);
            return required;
        } catch (...) { return 0; }
    }
}

// Caller retries when the required capacity changes between the two calls.
extern "C" __declspec(dllexport) std::uint32_t __cdecl OSFSettings_TestSnapshot(char* buffer, std::uint32_t capacity) noexcept
{
    return OSFSettings::TestHarness::Snapshot(buffer, capacity);
}
