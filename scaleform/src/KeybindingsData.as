package
{
    // Numeric binding policy shared by the page and offline fixture checks.
    public final class KeybindingsData
    {
        public static function bound(code:uint):Boolean { return code != 255 && code != 0xFFFFFFFF && code != 0x7FFFFFFF; }
        public static function modified(code:uint):Boolean { return code != 0 && bound(code); }
        public static function identity(context:uint, action:String):String { return context + ":" + action; }
        public static function bindingText(binding:Object):String
        {
            return (binding.aPCKeyName.length ? binding.aPCKeyName : binding.aButtonName).join(" + ");
        }
        public static function includes(record:Object, key:int, gamepad:Boolean = false):Boolean
        {
            if (!bound(record.key) || (record.device == 2) != gamepad) return false;
            if (gamepad) return record.key == key || modified(record.modifier) && record.modifier == key;
            return record.device == 0 && matches(record.key, key) || modified(record.modifier) && matches(record.modifier, key);
        }
        private static function matches(code:uint, key:int):Boolean
        {
            return code == key || code == 16 && (key == 160 || key == 161) ||
                code == 17 && (key == 162 || key == 163) || code == 18 && (key == 164 || key == 165);
        }
        public static function join(entries:Array, definitions:Array, records:Array, translate:Function):Array
        {
            var metadata:Object = {}, mappings:Object = {}, seen:Object = {}, result:Array = [];
            for each (var definition:Object in definitions) {
                if (definition.type == "hotkey" && definition.registered)
                    metadata[identity(0, definition.action)] = definition;
            }
            for each (var record:Object in records) {
                var id:String = identity(record.context, record.action) + ":" + (record.device == 2 ? 2 : 0);
                if (!mappings[id]) mappings[id] = [];
                mappings[id].push(record);
            }
            for each (var entry:Object in entries) {
                if (entry.bIsDivider || entry.uContextID != 0) continue;
                var actionID:String = identity(entry.uContextID, entry.sInputName);
                id = actionID + ":" + (entry.bGamepadEntry ? 2 : 0);
                if (seen[id]) continue;
                seen[id] = true;
                var owner:Object = metadata[actionID];
                var native:Object = {};
                for (var property:String in entry) native[property] = entry[property];
                var row:Object = {type:"hotkey", action:entry.sInputName, context:entry.uContextID, identity:id,
                    title:owner ? owner.title : translate(entry), mod:owner ? owner.mod : "", key:owner ? owner.key : entry.sInputName,
                    source:owner ? owner.modTitle : tr("bindings.game"), binding:native, editable:!entry.bReadOnly,
                    value:bindingText(entry.MainBinding), alternate:bindingText(entry.AltBinding),
                    records:mappings[id] || [], keybindings:true, defaultName:""};
                // Missing map records are unavailable, never an inferred unbound slot.
                row.available = mappings[id] != null;
                row.editable = row.editable && row.available;
                row.hint = row.available ? "" : tr("bindings.numericUnavailable");
                result.push(row);
            }
            return result;
        }
        public static function filter(rows:Array, query:String, source:String, key:int, gamepad:Boolean = false):Array
        {
            var result:Array = [], terms:Array = query.toLowerCase().split(/\s+/);
            for each (var row:Object in rows) {
                if (source != "all" && (source == "game" ? Boolean(row.mod) : row.mod != source)) continue;
                var hit:Boolean = key < 0;
                for each (var record:Object in row.records) if (includes(record, key, gamepad)) hit = true;
                if (!hit) continue;
                var text:String = (row.title + " " + row.source + " " + row.value + " " + row.alternate).toLowerCase();
                for each (record in row.records) text += " " + buttonName(record.key, record.device).toLowerCase() +
                    (modified(record.modifier) ? " " + buttonName(record.modifier, record.device == 2 ? 2 : 0).toLowerCase() : "");
                for each (var term:String in terms) if (term && text.indexOf(term) < 0) { hit = false; break; }
                if (hit) result.push(row);
            }
            return result;
        }
        public static function buttonName(code:uint, device:uint):String
        {
            return device == 2 ? GamepadMap.buttonName(code) : KeyboardMap.keyName(code, device);
        }
    }
}
