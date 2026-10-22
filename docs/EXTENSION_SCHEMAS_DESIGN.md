# Settings page extensions

Updated 2026-10-22. Implemented; this records the decisions behind it. The public contract is in
[Settings](SETTINGS.md) and [the SDK schema](../sdk/osf-settings.schema.json).

A mod that builds on another mod ships its own schema file and asks for its controls to be
shown on the base mod's page. Ownership never moves: the contributor keeps its own mod id,
its own values file, its own version and its own issue attribution. Only the page the rows
are drawn on changes.

## Player experience

A page that has contributions shows them as ordinary rows. Contributed controls that name a
group the host already has join that group under the host's heading; the rest appear as
further groups after the host's own, in mod-id order. The sidebar count for the page includes
them, so a page with no settings of its own can exist purely because something extended it.

Attribution is visible, not inferred. A group that exists only because a contributor added it
carries that contributor's name in its in-list heading; a group shared with the host does not,
because it has no single owner. The detail pane gains a **Provided by** line naming the
contributing mod whenever the selected row's owner is not the page's owner, which covers the
shared-group case and the single-group page that draws no heading at all. The row badge tint
is already derived from `mod + "/" + key`, so contributed rows read as a different family at a
glance without any new art. The page's detail card counts its contributors in a chip beside the
existing setting and hotkey counts, which is what tells a player a page is not all one mod's.

The sidebar keeps the bare group label. Section titles are short by design and are abbreviated
further under the `"Tab - "` convention, so the contributor's name belongs in the list header,
where there is room for it, and not in the navigation.

Reset is per row, as it is today (`B`), and resets the row against its own owner. Nothing
resets a whole page, so a contributor can never clear the host's values.

When the base mod is not installed, the contribution does not disappear: it renders as its own
page, titled by its own schema, and Mod Issues reports the missing dependency. The settings
still exist and stay readable through the API, because the values file never depended on the
host.

## Author contract

A root `extends` field naming the settings mod id of the page to join:

```json
{
  "title": "Base Mod - Combat Extras",
  "extends": "basemod",
  "version": "1.2.0",
  "groups": { "Combat": [ { "key": "recoil", "type": "float", "default": 0.5 } ] }
}
```

- The contributor's mod id is still its own filename. `GetFloat("basemod-combat", "recoil")`
  is the correct read; `basemod` never sees the key, and `ResetMod("basemod")` never touches it.
- Values live in `<Documents>/.../OSF/Settings/basemod-combat.json`, so uninstalling the
  contributor removes its rows and leaves the host's file untouched.
- The page owner's own groups always come first; contributions follow in mod-id order.
- Groups merge when their authored ids match exactly, case included. This matches the existing
  rule: group names are already compared exactly
  ([SettingsSchemaJson.cpp:113](../src/Settings/SettingsSchemaJson.cpp:113)) while setting keys
  fold ASCII case ([:208](../src/Settings/SettingsSchemaJson.cpp:208)). A merged group displays
  the **host's** label, including the host's translation; a group id the host does not have
  becomes a new group on the page. The `"Tab - "` sidebar-section convention keeps working, so
  a contributor can land inside an existing section by reusing the prefix. When two contributors
  land the same group id on a page the host does not share, that group has no single owner either:
  it keeps the first contributor's label and names nobody.
- Contributed groups follow the host's own groups in mod-id order, with each contributor's
  groups kept together in its authored order. Mod ids are filenames the author chooses, so
  naming a stack `basemod`, `basemod-combat`, `basemod-ui` already sorts it; there is no
  ordering field.
- Keys only have to be unique within the contributing mod, as now. Two mods may show rows with
  the same label on one page; identity stays `(modId, key)` everywhere, so nothing collides in
  storage or in the API. This is the main reason attribution ships in the first version.
- Actions, hotkeys and launchers need no new rules. Handlers are already registered per
  `(modId, id)`, so a contributed action button is owned and completed by the contributor.
  Hotkey `group` keeps today's restriction to groups declared in the same schema — cross-file
  group references cannot be validated at parse time, and a contributed hotkey landing in the
  contributor's own group on the host page is the expected result anyway.
- `extends` implies a dependency: if `dependencies` does not already name the host, the loader
  inserts `host: 0.0.0`. An author who needs a real floor writes it explicitly and gets the
  existing "update required" report instead of the plain "missing" one.
- `extends` must not name the schema's own id, `internal`, or a string that is not a valid mod
  id. Chains resolve to the first schema without `extends`, capped at 8 hops; a cycle or an
  over-long chain is a schema error and the contributor falls back to its own page.
- Runtime providers get this for free: `Register` parses the same schema text, so a provider
  may also contribute to a page. The host being static is irrelevant — nothing is written to it.

## Resolution

Page resolution is a post-load pass, not part of parsing, because it is cross-file.
`SettingsStore::LoadAll` ([SettingsStore.cpp:51](../src/Settings/SettingsStore.cpp:51)) finishes
building `m_mods` as it does now, then walks each schema's `extends` to a terminal page id and
stores the result. Unresolvable targets leave the page id equal to the mod id and push a
`SettingsLoadError`. The same pass runs after `RegisterProvider` and `UnregisterProvider`
([SettingsStore.cpp:105](../src/Settings/SettingsStore.cpp:105)), both of which already bump the
schema revision that dependency reports key off.

Merging itself is deliberately *not* done in the store. `ModSchema` stays one file's content, so
`FindMod`, `Set`, `Reset`, `ResetMod`, `Snapshot`, the registry and every API path keep their
present meaning. Merging happens where rows are built, which is the only place that wants it.

## Code touchpoints

| Where | Change |
| --- | --- |
| [SettingsSchema.h:117](../src/Settings/SettingsSchema.h:117) | `ModSchema` gains `std::string extends;` and a resolved `std::string page;` defaulting to `id` |
| [SettingsSchemaJson.cpp:156](../src/Settings/SettingsSchemaJson.cpp:156) | Parse and validate `extends` beside the `id` check; fold it into `ReadDependencies` at :158 |
| [SettingsStore.cpp:51](../src/Settings/SettingsStore.cpp:51) | Resolution pass after load and after each provider change, delegating to the shared `ResolvePages` |
| [SettingsSchema.cpp](../src/Settings/SettingsSchema.cpp) | `ResolvePages`, `PageOrder` and `FrameGroup`: page resolution, host-before-contributors ordering and per-group heading framing, shared by the menu and the exporter |
| [OSFSettingsMenu.cpp:436](../src/Menu/OSFSettingsMenu.cpp:436) | `modRow` emits `page`, `pageTitle` and `groupSource`; a row merging into a host group takes the host's `groupTitle` and an empty `groupSource`. Launcher rows carry their owner's page so an extension's interfaces open from it |
| [preview-rows.cpp:57](../tools/preview-rows.cpp:57) | The same three fields and the same host-label rule — this framing is duplicated from the menu, so lift it into a shared helper rather than writing it twice |
| [SettingsList.as:51](../scaleform/src/SettingsList.as:51) | The in-list section header appends `groupSource` when it is set, after the `"Tab - "` abbreviation runs |
| [OSFSettingsMenu.as:498](../scaleform/src/OSFSettingsMenu.as:498) | `refresh()` buckets mods by the page instead of `row.mod`; counts and chips follow |
| [OSFSettingsMenu.as:393](../scaleform/src/OSFSettingsMenu.as:393), [:524](../scaleform/src/OSFSettingsMenu.as:524), [:552](../scaleform/src/OSFSettingsMenu.as:552) | Row filter, `buildTabs` and `interfaceRows` compare the page, not the owner; `buildTabs` keeps using the bare `groupTitle` |
| [OSFSettingsMenu.as:610](../scaleform/src/OSFSettingsMenu.as:610) | Selection preservation keeps comparing `row.mod` — identity is still the owner |
| [SettingsDetails.as:62](../scaleform/src/SettingsDetails.as:62) | A **Provided by** label/value pair stacked like `rangeLabel`, shown when the row's owner is not the page |
| [ModIssue.cpp:88](../src/Diagnostics/ModIssue.cpp:88) | A missing host needs nothing: the implied dependency already reports it. `ExtensionIssues` covers the rest — an installed host the chain never reached, which only a cycle or an over-long chain produces — and [Compatibility.cpp](../src/Core/Compatibility.cpp) reports and clears both kinds together |
| [osf-settings.schema.json:39](../sdk/osf-settings.schema.json:39) | `extends` property referencing `#/definitions/modId` |
| [translations/en/osfsettings.json:20](../data/SFSE/Plugins/OSF/Settings/translations/en/osfsettings.json:20) | `menu.providedBy`, `menu.groupFrom`, the extension chips and the `issues.extensionUnresolved` strings; `ja` follows except the issue text |

Mod-author localization needs nothing: each side translates its own catalog, and the host-label
rule means a merged group never shows two translations of one heading.

## Deliberate non-changes

Registry ABI stays as it is. `ModView` describes one mod's data, and a page is presentation;
adding a page field would be an ABI bump for something no consumer has asked for. The
Keybindings page's source filter stays owner-based, because "which mod binds this key" is the
question it answers. Values storage, `ResetMod`, subscriptions and the Papyrus surface are all
untouched.

There is no host opt-in. Any mod can already retitle another mod's page with a drop-in
localization catalog, so requiring consent here would be inconsistent, and an opt-in would mean
contributions break against every base mod whose author has not heard of the feature.

## Decisions

**Groups merge, by exact id.** Without merging, an extras mod that adds one control to the base
mod's `Combat` group produces two `Combat` headings on one page, which reads as a bug. That case
is the whole point of the feature, so merging is not optional. Always giving a contributor its
own group would have removed the host-label question, but it trades a real problem for a
cosmetic one and makes a busy page a wall of near-duplicate headings. Exact, case-sensitive
matching follows the parser's existing treatment of group names, so the rule an author already
knows for one file is the rule across files.

**Attribution is a heading suffix plus a detail line, not one or the other.** The detail line
alone requires selecting a row to learn where it came from, which is backwards: a player looks
for the source of a row precisely when they have not yet picked it out. The heading answers it
for a whole block at once, but only for a group a single contributor owns — a merged group has
no single owner and would need to name all of them. So the heading carries the contributor for
contributed-only groups, and the detail line carries it for every contributed row, including
those in merged groups and on a page whose single group draws no header. Because `groupTitle` is
composed natively and shared with the sidebar, the contributor's name travels as its own
`groupSource` field rather than being baked into the title; that keeps navigation labels short
and leaves the `"Tab - "` abbreviation untouched.

**No `order` field.** Ordering across mutually unaware mods is a shared namespace with no
authority, and shared numeric namespaces degenerate — everyone picks 100 and the tie-break
decides anyway. The control already exists: a mod id is a filename the author chose, schemas
load in sorted filename order ([SettingsStore.cpp:33](../src/Settings/SettingsStore.cpp:33)), and
a related stack named `basemod`, `basemod-combat`, `basemod-ui` sorts the way it reads on disk.
The field is additive, so a real case can still add it later; nothing in this design forecloses
that.

## Tests

`extension_page_tests.cpp` runs with the schema suite and covers parsing and rejection of
`extends`, the implied dependency, resolution including cycles and a contributor that sorts before
its host, the three framing cases, page ordering, the issue split between a missing host and an
unresolved one, and a store round trip proving writes, resets and value files stay with the
contributor. `preview_rows_tests.py` covers the exporter's page and `groupSource` fields with two
schemas on one command line.

The `tests/menu` preview fixture the plan called for is deliberately missing: adding a contributed
page to `design-preview` changes entry counts that `PreviewChecks.as` asserts, and re-verifying
those needs the Ruffle harness this project does not run as part of ordinary verification. The
Scaleform changes are compile-verified only.
