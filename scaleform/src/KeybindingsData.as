package
{
    // Numeric binding policy shared by the page and offline fixture checks.
    public final class KeybindingsData
    {
        public static function bound(code:uint):Boolean { return code != 255 && code != 0xFFFFFFFF && code != 0x7FFFFFFF; }
        public static function modified(code:uint):Boolean { return code != 0 && bound(code); }
        public static function identity(context:uint, action:String):String { return context + ":" + action; }
        public static function includes(record:Object, key:int, gamepad:Boolean = false):Boolean
        {
            if (!bound(record.key)) return false;
            if (gamepad) return record.device == 2 && (record.key == key || modified(record.modifier) && record.modifier == key);
            if (record.device == 2) return false;
            return record.device == 0 && matches(record.key, key) || modified(record.modifier) && matches(record.modifier, key);
        }
        public static function bindingText(binding:Object):String
        {
            return binding.aPCKeyName.length ? binding.aPCKeyName.join(" + ") : binding.aButtonName.map(buttonName).join(" + ");
        }
        private static function buttonName(value:*, index:int, array:Array):String { return ControllerButtons.tokenName(String(value)); }
        private static function matches(code:uint, key:int):Boolean
        {
            return code == key || code == 16 && (key == 160 || key == 161) ||
                code == 17 && (key == 162 || key == 163) || code == 18 && (key == 164 || key == 165);
        }
        public static function join(entries:Array, definitions:Array, records:Array, translate:Function, gamepad:Boolean = false):Array
        {
            var metadata:Object = {}, mappings:Object = {}, seen:Object = {}, result:Array = [];
            for each (var definition:Object in definitions) {
                if (definition.type == "hotkey" && definition.registered)
                    metadata[identity(0, definition.action)] = definition;
            }
            for each (var record:Object in records) {
                var id:String = identity(record.context, record.action) + (record.device == 2 ? ":gp" : ":pc");
                if (!mappings[id]) mappings[id] = [];
                mappings[id].push(record);
            }
            for each (var entry:Object in entries) {
                if (entry.bIsDivider || entry.uContextID != 0) continue;
                id = identity(entry.uContextID, entry.sInputName);
                var mappingID:String = id + (entry.bGamepadEntry ? ":gp" : ":pc");
                if (seen[mappingID]) continue;
                seen[mappingID] = true;
                var owner:Object = metadata[id];
                var native:Object = {};
                for (var property:String in entry) native[property] = entry[property];
                var row:Object = {type:"hotkey", action:entry.sInputName, context:entry.uContextID, identity:id,
                    title:owner ? owner.title : translate(entry), mod:owner ? owner.mod : "", key:owner ? owner.key : entry.sInputName,
                    source:owner ? owner.modTitle : tr("bindings.game"), binding:native, editable:!entry.bReadOnly,
                    value:bindingText(entry.MainBinding), alternate:bindingText(entry.AltBinding), gamepad:Boolean(entry.bGamepadEntry),
                    records:mappings[mappingID] || [], keybindings:true, defaultName:""};
                // Missing map records are unavailable, never an inferred unbound slot.
                row.available = mappings[mappingID] != null;
                row.editable = row.editable && row.available;
                row.hint = row.available ? "" : tr("bindings.numericUnavailable");
                result.push(row);
            }
            // Hidden, bound owners are rejected by the native remap validator but
            // never appear in its Controls rows. Expose them without an editor.
            var reserved:Object = {};
            for each (record in records) {
                if (record.context != 0 || (record.device == 2) != gamepad || record.visibleInControls !== false ||
                    !bound(record.key) || record.slot > 1) continue;
                id = identity(record.context, record.action);
                mappingID = id + (gamepad ? ":gp" : ":pc");
                if (seen[mappingID]) continue;
                row = reserved[mappingID];
                if (!row) {
                    native = {sInputName:record.action,uContextID:record.context,sContextName:"MainGameplay",
                        bReadOnly:true,bIsDivider:false,bRequired:false,bGamepadEntry:gamepad,
                        MainBinding:{aPCKeyName:[],aButtonName:[]},AltBinding:{aPCKeyName:[],aButtonName:[]}};
                    row = {type:"hotkey",action:record.action,context:record.context,identity:id,title:record.action,
                        mod:"",key:record.action,source:tr("bindings.reservedSource"),binding:native,editable:false,
                        value:"",alternate:"",gamepad:gamepad,records:[],keybindings:true,defaultName:"",
                        available:true,reserved:true,hint:tr("bindings.reservedHint")};
                    reserved[mappingID] = row; result.push(row);
                }
                row.records.push(record);
                var names:Array = [];
                if (modified(record.modifier)) names.push(KeyboardMap.keyName(record.modifier, gamepad ? 2 : 0));
                names.push(KeyboardMap.keyName(record.key, record.device));
                var binding:Object = record.slot == 0 ? row.binding.MainBinding : row.binding.AltBinding;
                var text:String = names.join(" + ");
                binding.aPCKeyName = [binding.aPCKeyName.length ? binding.aPCKeyName[0] + " / " + text : text];
                row.value = bindingText(row.binding.MainBinding); row.alternate = bindingText(row.binding.AltBinding);
            }
            return result;
        }
        public static function filter(rows:Array, query:String, source:String, key:int):Array
        {
            var result:Array = [], terms:Array = query.toLowerCase().match(/\S+/g) || [];
            for each (var row:Object in rows) {
                if (source != "all" && (source == "game" ? Boolean(row.mod) : row.mod != source)) continue;
                var hit:Boolean = key < 0;
                if (!hit) for each (var record:Object in row.records) if (includes(record, key, Boolean(row.gamepad))) { hit = true; break; }
                if (!hit) continue;
                if (!terms.length) { result.push(row); continue; }
                var text:String = (row.title + " " + row.source + " " + row.value + " " + row.alternate).toLowerCase();
                for each (record in row.records) text += " " + KeyboardMap.keyName(record.key, record.device).toLowerCase() +
                    (modified(record.modifier) ? " " + KeyboardMap.keyName(record.modifier, record.device == 2 ? 2 : 0).toLowerCase() : "");
                for each (var term:String in terms) if (term && text.indexOf(term) < 0) { hit = false; break; }
                if (hit) result.push(row);
            }
            return result;
        }
    }
}
