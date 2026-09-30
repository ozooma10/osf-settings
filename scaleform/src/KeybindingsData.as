package
{
    // Numeric binding policy shared by the page and offline fixture checks.
    public final class KeybindingsData
    {
        public static function bound(code:uint):Boolean { return code != 255 && code != 0xFFFFFFFF && code != 0x7FFFFFFF; }
        public static function modified(code:uint):Boolean { return code != 0 && bound(code); }
        public static function identity(context:uint, action:String):String { return context + ":" + action; }
        public static function includes(record:Object, key:int):Boolean
        {
            if (!bound(record.key)) return false;
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
                var id:String = identity(record.context, record.action);
                if (!mappings[id]) mappings[id] = [];
                mappings[id].push(record);
            }
            for each (var entry:Object in entries) {
                if (entry.bIsDivider || entry.bGamepadEntry || entry.uContextID != 0) continue;
                id = identity(entry.uContextID, entry.sInputName);
                if (seen[id]) continue;
                seen[id] = true;
                var owner:Object = metadata[id];
                var native:Object = {};
                for (var property:String in entry) native[property] = entry[property];
                var row:Object = {type:"hotkey", action:entry.sInputName, context:entry.uContextID, identity:id,
                    title:owner ? owner.title : translate(entry), mod:owner ? owner.mod : "", key:owner ? owner.key : entry.sInputName,
                    source:owner ? owner.modTitle : tr("bindings.game"), binding:native, editable:!entry.bReadOnly,
                    value:entry.MainBinding.aPCKeyName.join(" + "), alternate:entry.AltBinding.aPCKeyName.join(" + "),
                    records:mappings[id] || [], keybindings:true, defaultName:""};
                // Missing map records are unavailable, never an inferred unbound slot.
                row.available = mappings[id] != null;
                row.editable = row.editable && row.available;
                row.hint = row.available ? "" : tr("bindings.numericUnavailable");
                result.push(row);
            }
            return result;
        }
        public static function filter(rows:Array, query:String, source:String, key:int):Array
        {
            var result:Array = [], terms:Array = query.toLowerCase().match(/\S+/g) || [];
            for each (var row:Object in rows) {
                if (source != "all" && (source == "game" ? Boolean(row.mod) : row.mod != source)) continue;
                var hit:Boolean = key < 0;
                if (!hit) for each (var record:Object in row.records) if (includes(record, key)) { hit = true; break; }
                if (!hit) continue;
                if (!terms.length) { result.push(row); continue; }
                var text:String = (row.title + " " + row.source + " " + row.value + " " + row.alternate).toLowerCase();
                for each (record in row.records) text += " " + KeyboardMap.keyName(record.key, record.device).toLowerCase() +
                    (modified(record.modifier) ? " " + KeyboardMap.keyName(record.modifier, 0).toLowerCase() : "");
                for each (var term:String in terms) if (term && text.indexOf(term) < 0) { hit = false; break; }
                if (hit) result.push(row);
            }
            return result;
        }
    }
}
