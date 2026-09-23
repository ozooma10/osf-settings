# Native action example

Build with `xmake build osfsettings-actions-example`. This development-only
target is excluded from normal builds, installs, and release packages.

To try it in your own development profile, install `OSFSettingsActionsExample.dll`
under `Data/SFSE/Plugins` and the adjacent schema under
`Data/SFSE/Plugins/OSF/Settings/schemas`, then restart the game. The confirmed
button logs its invocation and completes immediately. See [the action guide](../../docs/SETTINGS.md#action-buttons)
for deferred work, lifetime, and threading rules; the Papyrus example includes
a delayed completion.
