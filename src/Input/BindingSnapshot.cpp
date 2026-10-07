#include "BindingSnapshot.h"
#include "RE/B/BSService.h"
#include "RE/C/ControlMap.h"

namespace OSFSettings
{
    std::uint32_t RequestBindingSnapshot(const std::shared_ptr<BindingSnapshot>& mailbox)
    {
        const auto generation = mailbox->Begin();
        RE::BSService::TaskQueue::GetSingleton()->AddTask([weak = std::weak_ptr(mailbox), generation] {
            const auto target = weak.lock();
            if (!target || target->Read().generation != generation) return;
            const auto* map = RE::ControlMap::GetSingleton();
            constexpr auto context = RE::ControlMap::InputContextID::kMainGameplay;
            if (!map || !map->GetInputContext(context)) {
                target->Publish(generation, BindingSnapshot::Status::Unavailable);
                return;
            }
            std::vector<BindingRecord> records;
            for (const auto device : { RE::InputEvent::DeviceType::kKeyboard, RE::InputEvent::DeviceType::kMouse, RE::InputEvent::DeviceType::kGamepad }) {
                for (const auto& entry : map->GetMappings(context, device)) {
                    records.push_back({ std::string(entry.eventID.c_str()), static_cast<std::uint32_t>(context),
                        static_cast<std::uint32_t>(device), static_cast<std::uint32_t>(entry.bindingSlot),
                        entry.keyCode, entry.modifierKeyCode, entry.visibleInControls });
                }
            }
            target->Publish(generation, BindingSnapshot::Status::Ready, std::move(records));
        });
        return generation;
    }
}
