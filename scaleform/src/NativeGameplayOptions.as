package
{
    import flash.events.Event;
    import flash.text.TextField;
    import flash.utils.Dictionary;
    import flash.utils.getTimer;

    // The engine owns these values, their persistence and Papyrus notifications.
    public final class NativeGameplayOptions
    {
        private var manager:Object;
        private var global:Object;
        private var sources:Array;
        private var changed:Function;
        private var text:TextField = new TextField();
        private var entries:Array = [];
        private var pending:Object;
        private var deadline:int;
        private var timedOut:Boolean = false;
        public var rows:Array = [];
        public function get busy():Boolean { return pending != null && !timedOut; }

        public function NativeGameplayOptions(definition:Function, bridge:Object, notify:Function)
        {
            manager = definition("Shared.AS3.Data.BSUIDataManager");
            global = definition("Shared.GlobalFunc");
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
            var acknowledged:Boolean = false;
            for each (var entry:Object in entries) {
                if (entry.uType == 5) {
                    section = translated(entry.sText); sectionKeys = new Dictionary();
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
                    value:String(index(entry)), defaultKnown:false, nativeEditable:entry.bEnabled !== false,
                    editable:entry.bEnabled !== false && !pending,
                    gameplayID:id, gameplayType:uint(entry.uType)};
                rows.push(row);
                if (pending && pending.id == id && pending.value == index(entry)) acknowledged = true;
            }
            for each (row in rows) if (groups[row.mod].length == 1) row.modTitle = row.groupTitle;
            if (acknowledged) {
                // The change crosses two native queues. Save only after PEOData confirms it.
                pending = null; timedOut = false;
                for each (row in rows) row.editable = row.nativeEditable;
                manager.dispatchEvent(new Event("SettingsPanel_SaveSettings", true));
            }
            changed(acknowledged ? "gameplay.submitted" : "", false);
        }

        public function edit(row:Object, value:*):Boolean
        {
            if (pending || !row.editable || rows.indexOf(row) < 0) return false;
            var selected:Number = Number(value);
            if (!isFinite(selected) || selected != uint(selected) || selected >= row.options.length) return false;
            pending = {id:uint(row.gameplayID), value:uint(selected)};
            timedOut = false; deadline = getTimer() + 5000;
            for each (var other:Object in rows) other.editable = false;
            if (row.gameplayType == 3)
                manager.dispatchCustomEvent("SettingsPanel_CheckBoxChanged", {bChecked:selected != 0, uSettingID:pending.id, bIsPEO:true});
            else
                manager.dispatchCustomEvent("SettingsPanel_StepperChanged", {uIndex:uint(selected), uSettingID:pending.id, bIsPEO:true});
            changed("gameplay.applying", false);
            return true;
        }

        public function advance():void
        {
            if (busy && getTimer() >= deadline) {
                timedOut = true;
                changed("gameplay.timeout", true);
            }
        }

        public function dispose():void
        {
            manager.Unsubscribe("PEOData", updated);
        }
    }
}
