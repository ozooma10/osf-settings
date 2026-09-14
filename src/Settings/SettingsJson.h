#pragma once

#include <nlohmann/json_fwd.hpp>

#include "Settings/SettingsStore.h"

namespace OSFSettings::SettingsJson
{
	bool ValidateSchemaShape(const nlohmann::json& a_object, bool a_allowBuiltIn = false);
	std::optional<ModSchema> ParseSchema(const nlohmann::json& a_object, bool a_allowBuiltIn = false);
	std::optional<SettingValue> DecodeValue(const nlohmann::json& a_value);
	std::optional<SettingValue> NormalizeJsonValue(const SettingDefinition& a_definition, const nlohmann::json& a_value);
	nlohmann::json EncodeValue(const SettingValue& a_value);
	std::string DumpValue(const SettingValue& a_value);
	SettingsStore::SetResult ApplyValue(SettingsStore& a_store, std::string_view a_mod, std::string_view a_key, std::string_view a_json);
}
