package
{
    import flash.events.Event;
    import flash.text.TextField;
    import flash.utils.Dictionary;

    // The engine owns these values, their persistence and Papyrus notifications.
    public final class NativeGameplayOptions
    {
        private var manager:Object;
        private var global:Object;
        private var sources:Array;
        private var bridge:Object;
        private var changed:Function;
        private var text:TextField = new TextField();
        private var entries:Array = [];
        public var rows:Array = [];

        public function NativeGameplayOptions(definition:Function, bridge:Object, notify:Function)
        {
            // HTML line breaks are discarded when SetText uses a single-line field.
            text.multiline = true;
            manager = definition("Shared.AS3.Data.BSUIDataManager");
            global = definition("Shared.GlobalFunc");
            this.bridge = bridge;
            sources = bridge.getGameplaySources() as Array || [];
            changed = notify;
            manager.Subscribe("PEOData", updated);
            manager.dispatchEvent(new Event("SettingsPanel_OpenSettings", true));
            manager.dispatchCustomEvent("SettingsPanel_OpenCategory", {categoryID:0});
        }

        private function translated(value:*, substitutions:Array = null):String
        {
            global.SetText(text, value == null ? "" : String(value), true, false, 0, false, 0, substitutions);
            return text.text;
        }

        private function sourceFor(id:uint):Object
        {
            for each (var source:Object in sources)
                if (uint(id & uint(source.mask)) == uint(source.prefix)) return source;
            return null;
        }

        private function index(entry:Object):uint
        {
            return entry.uType == 3 ? (entry.checkBoxData.bChecked ? 1 : 0) : uint(entry.stepperData.uIndex);
        }

        private function updated(event:Event):void
        {
            var data:Object = Object(event).data;
            entries = data && data.aGeneralSettingsList is Array ? data.aGeneralSettingsList : [];
            rows = [];
            var seen:Dictionary = new Dictionary(), groups:Dictionary = new Dictionary();
            var section:String = "", sectionKeys:Dictionary = new Dictionary();
            for each (var entry:Object in entries) {
                if (entry.uType == 5) {
                    // Separator-only groups can contain spaces or invisible padding.
                    // Normalize those names so the source filename fallback also covers them.
                    section = translated(entry.sText).replace(/^[\s\u00A0\u200B\uFEFF]+|[\s\u00A0\u200B\uFEFF]+$/g, "");
                    sectionKeys = new Dictionary();
                    continue;
                }
                if (!entry.bIsPEO || (entry.uType != 1 && entry.uType != 2 && entry.uType != 3)) continue;
                var id:uint = uint(entry.uID), source:Object = sourceFor(id);
                if (!source || seen[id]) continue;
                seen[id] = true;
                var file:String = String(source.file), mod:String = "gameplay/" + file.toLowerCase();
                var key:String = uint(id & ~uint(source.mask)).toString(16);
                // Headers need not carry form IDs. Anchor each section to its first option.
                if (!sectionKeys[mod]) sectionKeys[mod] = "option-" + key;
                var group:String = sectionKeys[mod];
                if (!groups[mod]) groups[mod] = [];
                if (groups[mod].indexOf(group) < 0) groups[mod].push(group);
                var labels:Array = entry.uType == 3 ? [
                    entry.checkBoxData.bOffCustomText || "$OFF", entry.checkBoxData.bOnCustomText || "$ON"
                ] : entry.stepperData.aStepperOptions as Array;
                if (!labels || !labels.length || index(entry) >= labels.length) continue;
                var choices:Array = [], substitutions:Array = [];
                var rewards:Array = entry.peoData ? entry.peoData.aRewards as Array : null;
                var descriptions:Array = entry.peoData ? entry.peoData.aDescriptions as Array : null;
                for (var i:int = 0; i < labels.length; ++i) {
                    var label:String = translated(labels[i]);
                    var reward:Number = rewards && i < rewards.length ? Number(rewards[i]) : 0;
                    var xp:String = (reward >= 0 ? "+" : "") + reward + "% " + translated("$XP");
                    choices.push({value:String(i), label:label + (reward ? " (" + xp + ")" : "")});
                    if (descriptions && i < descriptions.length)
                        substitutions.push(global.DoSubstitutions(descriptions[i], [label, "(" + xp + ")"]));
                }
                var row:Object = {mod:mod, modTitle:file, modDescription:file, group:group,
                    groupTitle:section || file, key:key, title:translated(entry.sText),
                    hint:translated(entry.sDescription, substitutions), type:"enum", options:choices,
                    value:String(index(entry)), defaultKnown:false,
                    editable:entry.bEnabled !== false && source.editable === true,
                    gameplayID:id, gameplayType:uint(entry.uType)};
                rows.push(row);
            }
            for each (row in rows) if (groups[row.mod].length == 1) row.modTitle = row.groupTitle;
            changed("", false);
        }

        public function edit(row:Object, value:*):Boolean
        {
            if (!row.editable || rows.indexOf(row) < 0) return false;
            var selected:Number = Number(value);
            if (!isFinite(selected) || selected != uint(selected) || selected >= row.options.length) return false;
            if (!bridge.commitGameplayChange(uint(row.gameplayID), uint(row.gameplayType), uint(selected))) return false;
            // The native value and save dispatch have completed; PEOData remains the live feed.
            row.value = String(uint(selected));
            changed("gameplay.saved", false);
            return true;
        }

        public function dispose():void
        {
            manager.Unsubscribe("PEOData", updated);
        }
    }
}
