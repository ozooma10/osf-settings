#include "GameplayOptionCommit.h"

#include "Core/Compatibility.h"
#include "RE/B/BGSGameplayOption.h"
#include "RE/S/SettingsDataModel.h"
#include "RE/U/UI.h"

namespace OSFSettings::GameplayOptionCommit
{
    bool Commit(std::uint32_t id, std::uint32_t type, std::uint32_t value)
    {
        if ((type != 1 && type != 2 && type != 3) || (type == 3 && value > 1)) return false;
        const auto* ui = RE::UI::GetSingleton();
        auto* model = RE::SettingsDataModel::GetSingleton();
        if (!ui || !model || ui->IsMenuOpen("MainMenu") || ui->IsMenuOpen("LoadingMenu")) return false;
        const auto* option = RE::TESForm::LookupByID<RE::BGSGameplayOption>(id);
        if (!option) return false;
        if (option->GetIndex() == value) return true;

        model->SetGameplayOptionIndex(id, value);
        model->SaveSettings();
        const bool applied = option->GetIndex() == value;
        REX::INFO("Gameplay option commit: id={:08X} index={} applied={}", id, value, applied);
        return applied;
    }
}
