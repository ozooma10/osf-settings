# Vanilla key names

Schema key defaults accept the names from Starfield's embedded keyboard table,
case-insensitively. Examples include `Caps Lock`, `PgUp`, `PgDn`, `PrtScn`,
`L Ctrl`, `R Shift`, `NumPad+`, and literal punctuation such as `;`, `[` and `=`.
Names use vanilla's spelling: `PgUp` rather than `PageUp`, `L Ctrl` rather than
`LCtrl`, and `F4` rather than `VK_F4`. Custom aliases and enum reflection are
removed. Keys absent from vanilla's table, such as media keys, can still use
numeric defaults. Saved values and the SDK continue to use numeric Windows
virtual-key codes.

## Engine source

Static inspection of the unpacked Starfield 1.16.244.0 executable established:

| Component | RVA | Address Library ID |
| --- | --- | --- |
| Embedded UTF-16 keyboard table | `0x04A56010` | `361050` |
| Console hotkey constructor/parser | `0x01E9F440` | `113560` |
| Input device scalar deleting destructor | `0x022FA5B0` | `124249` |
| Code-to-display-name lookup, device vtable slot 4 | `0x022FBE40` | `124283` |
| Keyboard name lookup, device vtable slot 5 | `0x022FBF50` | `124284` |
| Secondary-code lookup, device vtable slot 6 | `0x022FC060` | `124285` |
| Input device manager pointer | `0x05FD9CD8` | `937644` |

The console INI reader's `[Hotkeys]` path calls the hotkey constructor. It strips
`Shift-`, `Ctrl-`, and `Alt-` prefixes in that order, then tries the keyboard,
mouse, and gamepad name lookup methods. Keyboard lookup compares names with
`_stricmp` and returns the virtual-key code, or `0xFFFFFFFF` for no match.

The keyboard constructor at RVA `0x022FC520` loads the embedded table through
`0x022FB890` and `0x022FB950`. The row parser at `0x022FBBA0` converts names to
UTF-8 and reads the second tab-separated column as hexadecimal VK. The optional
third column is a separate decimal code and is not used by name lookup. For
example, `L Ctrl` has VK `0xA2`, while its third column is `17`.

The table contains 115 names. Address Library ID `361050` also resolves to the
byte-identical table in 1.16.242.0, at RVA `0x04A5A020`. Its 2,778 bytes, including
the terminating UTF-16 NUL, have SHA-256:

```text
c3b139f9c26cfec47bfe10dde454d73421d4bc3c1c31feb0c742ca1ab26393b4
```

## CommonLibSF binding

The local CommonLibSF additions expose all nine virtual slots on
`RE::BSInputDevice`. The keyboard inherits the three lookup methods:

```cpp
if (const auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
    if (const auto* keyboard = manager->GetKeyboard()) {
        const auto key = keyboard->GetKeyCodeFromName("F4");  // 0x73
        // 0xFFFFFFFF means the name was not found.
        RE::BSFixedStringCS name;
        if (keyboard->GetKeyNameFromCode(key, name)) {
            // Copy name.c_str() for the menu's display label.
        }
    }
}
```

This dispatches through native vtable slot 5 (+0x28). There is no copied keyboard
table, custom parser, or cache. The name must be non-null and NUL-terminated.
The singleton uses Address Library ID `937644`. Calls on a live device dispatch
through its native vtable. The base destructor and three lookup methods also
have native forwarding definitions, using the IDs above, for qualified base
calls. The names describe the reverse-engineered behavior; they are not
recovered Bethesda symbols.

| Slot | Declaration | Native contract |
| --- | --- | --- |
| 0 | `~BSInputDevice()` | Native cleanup; the forwarding body passes zero deletion flags. |
| 1 | `void Initialize()` | Pure virtual; called after manager device creation. |
| 2 | `void Process(float deltaTime)` | Pure virtual; manager forwards frame time in XMM1. |
| 3 | `void Release()` | Pure virtual; manager calls this before destruction. |
| 4 | `bool GetKeyNameFromCode(uint32_t, BSFixedStringCS&) const` | Copies the display name; failure preserves the output. |
| 5 | `uint32_t GetKeyCodeFromName(const char*) const` | Finds the original table name; failure returns `0xFFFFFFFF`. |
| 6 | `bool GetMappedKeyCode(uint32_t, uint32_t&) const` | Reads the optional third table column; failure preserves the output. |
| 7 | `bool IsEnabled() const` | Base returns true; gamepad override checks connection state and device ID. |
| 8 | `void Reset()` | Pure virtual; clears device input state. |

The manager calls slots 1, 2, 3 and 8 at RVAs `0x022D73BB`, `0x022D750F`,
`0x022D7450` and `0x022D77BE`. Keyboard initialization tail-calls slot 8, which
clears its two 256-byte state buffers; gamepad initialization allocates state
buffers and then resets them. The gamepad slot-7 implementation at `0x022FAA40`
requires a nonzero connection byte at +0xB8 and a device ID other than -1 at +0xC.
The pure virtual declarations match the base vtable, rather than substituting
empty lifecycle implementations.

Slot 4 reads the code-to-name map and manages the output string's reference
count. The keyboard's layout-substitution routine at `0x022FC620` updates that
map, using UTF-8 case-sensitive pooled strings, while leaving the original
name-to-code map intact. A display label is therefore not guaranteed to be a
valid INI name. Slot 6 is a secondary code, not a scan-code conversion: L Ctrl
maps from `0xA2` to generic Ctrl `0x11`; an existing row without a third column
succeeds with zero.

The 1.16.244 RTTI and constructors establish:

- `BSInputDevice` is 0x80 bytes, with nine virtual slots.
- `BSKeyboardDevice` derives from it at +0 and inherits slot 5 unchanged.
  `BSPCKeyboardDevice` also inherits that method.
- `BSInputEventSingleUser` contains `BSInputEventReceiver` at +0 and
  `BSInputEventUser` at +0x10; its size is 0x50.
- `BSInputDeviceManager` contains `BSTSingletonSDM` at +0 and
  `BSInputEventSingleUser` at +0x10; its size is 0xD8.
- Five device pointers begin at manager +0x68. The keyboard is the first.
  Other device slots and unknown members remain unnamed.

The headers assert these sizes and the device-array offset. Unknown fields
remain opaque, and there is no plugin-side device constructor. Use the existing
engine-owned devices. The vtable layout and all four forwarding IDs were also
checked against the unpacked 1.16.242 executable and its matching Address Library.

## OSF Settings integration

The current startup code initializes schemas and the service during plugin
`OnLoad`. Both key-name adapters check for a missing manager or keyboard.
The native keyboard is absent at this early registration phase (confirmed in
the first production run). Before it exists, `KeyCodeFromName` reads the native
UTF-16 table at ID 361050, after checking executable version, RVA, and all 2778
bytes against the verified FNV-1a fingerprint `EE1F5EE164E5555F`. It reads the
name and hexadecimal VK columns directly, without a copied alias table or
cached name index. The embedded names are ASCII. Unsupported versions or
changed table bytes fail closed. Numeric defaults and `UNBOUND` need no lookup.
Consumers must check `IsReady()` before reading settings.

Once the keyboard exists, `KeyCodeFromName` uses its native virtual method.
It handles OSF's `UNBOUND` spelling, rejects empty names and embedded NULs,
terminates the input string view, and translates native `0xFFFFFFFF` to
`std::nullopt`. Both paths use vanilla names, including F1-F24.
Only single keyboard keys are supported. `Esc` resolves to Escape but remains
reserved for capture cancellation. `KeyName` calls the native code-to-name
method directly and copies its UTF-8 result. `UNBOUND` and numeric fallbacks
remain plugin policy. This removes the Windows display API calls and the
Pause/NumLock scan-code special cases.

Native tests substitute the manager, virtual lookups, and the two string-pool
relocations needed to own display text. They verify forwarding, UTF-8 copying,
null/empty/failure handling, schema validation, persistence, and the SDK.
Pure early-table tests also cover exact names, case handling, punctuation,
the hexadecimal column, rejection of aliases, and malformed codes.
They do not execute native device lifecycle functions.
Static inspection and native tests do not verify the live relocation or startup
timing. A fresh game session must confirm a successful keyboard lookup during
schema loading, the absence of schema errors, named defaults such as F4 and
L Ctrl, and the menu's labels for letters, function keys, modifiers, and the
active keyboard layout.
