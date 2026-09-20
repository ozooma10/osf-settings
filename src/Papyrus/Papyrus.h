#pragma once

namespace OSFSettings::Papyrus
{
    bool Install(); // Hook native registration during plugin load.
    bool RegisterSinks(); // Manage subscriptions across save/load and menu transitions.
}
