#include "GameplayOptionCommit.h"

#include "Core/Compatibility.h"
#include "RE/B/BGSGameplayOption.h"
#include "RE/S/SettingsDataModel.h"
#include "RE/U/UI.h"

namespace OSFSettings::GameplayOptionCommit
{
    bool Commit(std::uint32_t id, std::uint32_t type, std::uint32_t value)
    {
        if ((type != 1 && type != 2 && type != 3) || (type == 3 && value > 1)) {
            REX::WARN("Gameplay option commit rejected: id={:08X} type={} index={} invalid option type or checkbox index", id, type, value);
            return false;
        }
        const auto* ui = RE::UI::GetSingleton();
        auto* model = RE::SettingsDataModel::GetSingleton();
        if (!ui || !model) {
            REX::WARN("Gameplay option commit rejected: id={:08X} UI available={} settings model available={}", id, ui != nullptr, model != nullptr);
            return false;
        }
        if (ui->IsMenuOpen("MainMenu") || ui->IsMenuOpen("LoadingMenu")) {
            REX::WARN("Gameplay option commit rejected: id={:08X} main or loading menu is open", id);
            return false;
        }
        const auto* option = RE::TESForm::LookupByID<RE::BGSGameplayOption>(id);
        if (!option) {
            REX::WARN("Gameplay option commit rejected: id={:08X} is not a loaded gameplay option", id);
            return false;
        }
        if (option->GetIndex() == value) {
            REX::DEBUG("Gameplay option commit: id={:08X} index={} already selected", id, value);
            return true;
        }

        model->SetGameplayOptionIndex(id, value);
        model->SaveSettings();
        const bool applied = option->GetIndex() == value;
        REX::INFO("Gameplay option commit: id={:08X} index={} applied={}", id, value, applied);
        return applied;
    }
}
