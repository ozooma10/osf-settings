# Runtime settings providers

Include [OSFSettings_Providers.h](../sdk/OSFSettings_Providers.h) and acquire providers ABI 1.0 at SFSE `kPostLoad` or later. This optional service registers runtime definitions with caller-owned persistence while keeping editing, validation, notifications, and typed reads in OSF Settings.

`Register(mod, schemaJson, valuesJson, save, context, &registration)` accepts the normal schema and a flat initial value object. Missing values use defaults. Keys are VK integers. Only value settings belong in a provider; use the existing APIs for actions, launchers, and ControlMap hotkeys.

Start with a zero registration token. Successful registration copies the schema and values, retains the save callback/context, publishes the provider in the menu, and notifies subscribers. Reuse the returned token to replace a definition for the same mod; still-valid current values survive. A different owner or a static schema returns `AlreadyRegistered`. Failed registration leaves ownership unchanged. `Unregister` removes only the provider with that token, waits for any save under the transaction lock, and makes its callback context safe to release.

The save callback receives a complete normalized flat value object. Return true only after persistence succeeds. It runs synchronously under the settings transaction lock: do not call any Settings API from it. A false return produces `SaveFailed`, leaving the current value and notifications unchanged. Registration/replacement itself does not call save or create a value file. The provider owns schema migration and recovery of its initial persistence input.

`SubscribeKey` observes a key setting's gameplay press without consuming it or installing a ControlMap action. It can be registered before the setting exists. Changes to the binding take effect on the next press; shared hotkey blocks and gameplay/menu gates apply. Callbacks run inline on input dispatch and should return promptly. Unsubscribe waits for an in-flight callback; self-unsubscribe is supported. A provider may use ordinary value subscriptions independently.

Key schemas may opt into `allowMouse: true` to accept the five physical mouse buttons. Defaults may use `MOUSE1` through `MOUSE5`, or VK integers `1, 2, 4, 5, 6`. The default remains keyboard-only. Mouse capture releases input back to the menu after the candidate button is released so the confirmation controls still work.
