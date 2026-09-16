#include "NativeHotkeys.h"
#include "HotkeyService.h"
#include "RE/B/BSInputEventReceiver.h"
#include "RE/B/BSService.h"
#include "RE/C/ControlMap.h"
#include "RE/C/ControlsRemappedEvent.h"
#include "REL/THook.h"
#include "REL/Trampoline.h"
#include <Windows.h>
#undef ERROR
#include <map>
#include <memory>
#include <optional>

namespace OSFSettings::NativeHotkeys
{
    namespace
    {
        struct Hooks
        {
            using Receiver = REL::THookVFT<void(RE::BSInputEventReceiver*, const RE::InputEvent*)>;
            std::optional<Receiver> receiver;
            void (*remapped)(const RE::ControlsRemappedEvent&){};
            RE::BSFixedStringW* (*translate)(void*, RE::BSFixedStringW*, const wchar_t*){};
            std::vector<RE::ControlMap::KeyboardAction> mappings;
            std::map<std::wstring, std::wstring, std::less<>> labels;
            std::atomic_bool ready{}, started{};
            bool installed{};
        };
        // Hooks and their backing data remain valid through engine shutdown.
        auto* g_hooks = new Hooks;

        class MenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent&, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                HotkeyService::Get().Invalidate();
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // Read only on the verified native game-thread drain, never an input worker.
        bool GameplayAllowed()
        {
            if (!g_hooks->ready.load() || !g_hooks->started.load()) return false;
            const auto* ui = RE::UI::GetSingleton();
            const auto* main = RE::Main::GetSingleton();
            const auto* player = RE::PlayerCharacter::GetSingleton();
            if (!ui || !main || !player || !player->parentCell || main->quitGame || main->resetGame ||
                main->isGameMenuPaused || ui->pauseRequestCount) return false;
            DWORD process{};
            ::GetWindowThreadProcessId(::GetForegroundWindow(), &process);
            if (process != ::GetCurrentProcessId()) return false;
            for (const auto& menu : ui->menuArray) {
                if (menu && ((menu->flags & (RE::IMenu::kModal | RE::IMenu::kBlocksLowerMenuInput)) != 0 ||
                    menu->menuName == std::string_view{ "Console" })) return false;
            }
            return true;
        }

        void QueueActions()
        {
            // Defer out of input dispatch; the native queue can otherwise run inline.
            SFSE::GetTaskInterface()->AddTask([] {
                auto* queue = RE::BSService::TaskQueue::GetSingleton();
                if (!queue || !RE::BSService::TaskQueue::IsQueueEnabled()) {
                    HotkeyService::Get().Invalidate();
                    return;
                }
                queue->AddTask([] {
                    auto& service = HotkeyService::Get();
                    if (!RE::BSService::TaskQueue::IsQueueEnabled() ||
                        RE::BSService::TaskQueue::GetDrainOwnerThreadID() != ::GetCurrentThreadId()) {
                        service.Invalidate();
                        return;
                    }
                    service.Dispatch(GameplayAllowed);
                });
            });
        }

        std::wstring Wide(std::string_view text)
        {
            const auto length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                static_cast<int>(text.size()), nullptr, 0);
            std::wstring result(length, L'\0');
            if (length) ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                static_cast<int>(text.size()), result.data(), length);
            return result;
        }
        void InputHook(RE::BSInputEventReceiver* receiver, const RE::InputEvent* head)
        {
            HotkeyService::Get().ProcessInput(head, [&] { (*g_hooks->receiver)(receiver, head); });
        }
        void RemappedHook(const RE::ControlsRemappedEvent& event)
        {
            g_hooks->remapped(event);
            HotkeyService::Get().Invalidate();
        }
        void ParseHook(RE::ControlMap* map, const char* text)
        {
            auto& service = HotkeyService::Get();
            service.Invalidate();
            g_hooks->ready.store(false, std::memory_order_release);
            const bool parsed = map->ParseMappings(text, g_hooks->mappings, RE::ControlMap::InputContextID::kMainGameplay, "Jump");
            if (!parsed) {
                map->ParseMappings(text);
                REX::ERROR("Hotkeys: MainGameplay/Jump was not found in native defaults");
            }
            service.SetAvailable(parsed);
            g_hooks->ready.store(parsed, std::memory_order_release);
        }
        RE::BSFixedStringW* TranslateHook(void* translator, RE::BSFixedStringW* result, const wchar_t* key)
        {
            if (key && result) {
                const auto label = g_hooks->labels.find(std::wstring_view{ key });
                if (label != g_hooks->labels.end()) return std::construct_at(result, label->second.c_str());
            }
            return g_hooks->translate(translator, result, key);
        }
    }

    bool Install()
    {
        if (g_hooks->installed) return true;
        auto& service = HotkeyService::Get();
        if (!service.Configure(QueueActions)) return false;
        service.SetAvailable(false);
        const auto actions = service.Actions();
        if (!actions.empty()) {
            std::vector<RE::ControlMap::KeyboardAction> mappings;
            std::map<std::wstring, std::wstring, std::less<>> labels;
            for (const auto& action : actions) {
                auto label = Wide(action.label);
                if (label.empty()) return false;
                const auto token = L"$MainGameplay_" + Wide(action.eventName);
                labels.emplace(token, label);
                labels.emplace(token + L"_KBM", std::move(label));
                mappings.push_back({ action.eventName, action.defaultKey, 0x401 });
            }
            g_hooks->mappings = std::move(mappings);
            g_hooks->labels = std::move(labels);
            g_hooks->receiver.emplace("NativeHotkeys::Input", RE::VTABLE::UI[10], 1, InputHook);
            if (!g_hooks->receiver->Enable()) return false;
            auto& trampoline = REL::GetTrampoline();
            trampoline.write_detour<6>(RE::ID::ControlsRemappedEvent::Dispatch.address(), RemappedHook, g_hooks->remapped);
            trampoline.write_detour<5>(RE::ID::BSScaleformTranslator::ScaleformImpl::Translate.address(), TranslateHook, g_hooks->translate);
            REL::Relocation<std::uintptr_t>{ RE::ID::ControlMap::LoadMappings }.write_call<5, 0x45>(ParseHook);
        }
        service.SetAvailable(true);
        g_hooks->installed = true;
        REX::INFO("Hotkeys: installed {} native action(s)", actions.size());
        return true;
    }
    void Start()
    {
        if (g_hooks->started.exchange(true)) return;
        RE::UI::GetSingleton()->RegisterSink(new MenuEvents);
    }
}
