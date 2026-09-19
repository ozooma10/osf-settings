package
{
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.FocusEvent;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.text.TextField;
    import flash.ui.Keyboard;
    import flash.utils.getTimer;

    public final class KeybindingsPage extends Sprite
    {
        public var rows:Array = [];
        public var state:String = "loading";
        public var selectedKey:int = -1;
        public var source:String = "all";
        public var search:TextField;
        private var keyboard:KeyboardMap;
        private var sourceLabel:TextField;
        private var filterLabel:TextField;
        private var detail:TextField;
        private var legend:TextField;
        private var bridge:Object;
        private var editor:NativeHotkeysList;
        private var changed:Function;
        private var leaveSearch:Function;
        private var definitions:Array = [];
        private var sources:Array = [{id:"all",title:"All sources"},{id:"game",title:"Game"}];
        private var generation:uint;
        private var revision:uint;
        private var requestedAt:int;
        private var nextPoll:int;
        private var frozen:Boolean;

        public function KeybindingsPage(code:Object, nativeEditor:NativeHotkeysList, notify:Function, resultsFocus:Function)
        {
            bridge = code; editor = nativeEditor; changed = notify; leaveSearch = resultsFocus;
            keyboard = new KeyboardMap(selectKey); keyboard.x = MenuStyle.LEFT; keyboard.y = 270; addChild(keyboard);
            search = field("",MenuStyle.LEFT + 12,536,560,38,CONFIG::largeText ? 28 : 25);
            nameField(search,"bindingSearch"); search.type = "input"; search.selectable = true; search.mouseEnabled = true;
            search.maxChars = 128;
            search.addEventListener(Event.CHANGE,filtersChanged);
            search.addEventListener(FocusEvent.FOCUS_IN,focusChanged);
            search.addEventListener(FocusEvent.FOCUS_OUT,focusChanged);
            sourceLabel = field("",740,536,570,38,CONFIG::largeText ? 27 : 24);
            nameField(sourceLabel,"bindingSource").addEventListener(MouseEvent.CLICK,nextSource); sourceLabel.mouseEnabled = true;
            var clear:TextField = field("CLEAR FILTERS",1480,536,364,38,CONFIG::largeText ? 27 : 24);
            nameField(clear,"clearBindingFilters").addEventListener(MouseEvent.CLICK,clearFilters); clear.mouseEnabled = true;
            filterLabel = field("Search action, mod or key",MenuStyle.LEFT,580,1000,31,20,MenuStyle.MUTED);
            legend = field("Game: grey   Mod: orange   Mixed: split   Shared key: diamond",MenuStyle.LEFT,884,1728,28,20,MenuStyle.MUTED);
            detail = field("",1210,635,634,241,CONFIG::largeText ? 27 : 24);
            detail.multiline = true; detail.wordWrap = true; detail.mouseEnabled = true;
            detail.addEventListener(MouseEvent.MOUSE_WHEEL,function(event:MouseEvent):void { detail.scrollV -= event.delta; event.stopPropagation(); });
            field("ACTION / SOURCE",MenuStyle.LEFT,604,490,30,20,MenuStyle.MUTED);
            field("PRIMARY",MenuStyle.LEFT + 566,604,210,30,20,MenuStyle.MUTED);
            field("ALTERNATE",MenuStyle.LEFT + 804,604,210,30,20,MenuStyle.MUTED);
            graphics.lineStyle(1,MenuStyle.LINE); graphics.drawRect(MenuStyle.LEFT,532,594,42);
            graphics.drawRect(728,532,680,42);
            visible = false;
        }
        private function field(text:String,x:Number,y:Number,w:Number,h:Number,size:Number,color:uint = 0xF1F2EC):TextField
        {
            var result:TextField = MenuStyle.field(text,x,y,w,h,size,color,true); addChild(result); return result;
        }
        private function nameField(field:TextField,name:String):Sprite
        {
            var wrapper:Sprite = new Sprite(); wrapper.name = name;
            addChild(wrapper); wrapper.addChild(field); return wrapper;
        }
        public function get searching():Boolean { return visible && stage && stage.focus == search; }
        public function open(metadata:Array):void
        {
            definitions = metadata; visible = true; editor.open(); request();
        }
        public function close():void
        {
            if (searching) leaveSearch();
            visible = false; ++generation;
        }
        private function request():void
        {
            revision = editor.revision; state = "loading"; rows = [];
            generation = bridge.requestBindings(); requestedAt = getTimer(); nextPoll = 0;
        }
        public function advance(metadata:Array, busy:Boolean):void
        {
            if (!visible) return;
            frozen = busy; mouseChildren = !busy;
            if (busy) return;
            definitions = metadata;
            if (revision != editor.revision) { request(); changed(); }
            if (state != "loading" || getTimer() < nextPoll) return;
            nextPoll = getTimer() + 100;
            var snapshot:Object = bridge.pollBindings();
            if (snapshot && snapshot.generation == generation && snapshot.state != "loading") {
                if (snapshot.state == "ready" && editor.ready) {
                    rows = KeybindingsData.join(editor.bindings,definitions,snapshot.records,editor.title);
                    state = "ready"; updateSources(); changed(); return;
                }
                if (snapshot.state == "unavailable") { state = "unavailable"; changed(); return; }
            }
            if (getTimer() - requestedAt > 5000) { state = "unavailable"; changed(); }
        }
        private function updateSources():void
        {
            sources = [{id:"all",title:"All sources"},{id:"game",title:"Game"}];
            var seen:Object = {};
            for each (var row:Object in rows) if (row.mod && !seen[row.mod]) {
                sources.push({id:row.mod,title:row.source}); seen[row.mod] = true;
            }
            if (source != "all" && source != "game" && !seen[source]) source = "all";
        }
        public function filtered():Array { return KeybindingsData.filter(rows,search.text,source,selectedKey); }
        public function get emptyText():String
        {
            return state == "loading" ? "Loading native bindings..." : state == "unavailable" ?
                "Native binding data unavailable. Reopen Keybindings to retry." : "No actions match these filters.";
        }
        public function showSelection(row:Object):void
        {
            keyboard.update(rows,filtered(),row,selectedKey);
            var title:String = "All sources";
            for each (var choice:Object in sources) if (choice.id == source) title = choice.title;
            MenuStyle.fit(sourceLabel,"SOURCE: " + title + "  >");
            MenuStyle.setText(filterLabel,(search.text ? "Search: " + search.text : "Search action, mod or key") +
                (selectedKey >= 0 ? "   |   Key: " + KeyboardMap.keyName(selectedKey,0) + " (click again to clear)" : ""));
            var text:String = row ? row.title + "\n" + row.source + (row.binding.bRequired ? "  |  Required" : "") + (row.binding.bReadOnly ? "  |  Read only" : "") : "Select an action";
            if (row) text += "\n" + (row.available ? (row.value || "Unbound") + "  /  " + (row.alternate || "Unbound") : "Binding data unavailable");
            if (row && row.potential) text += "\nPotential conflict: another action has the same device, key and modifier.";
            else if (row && row.shared) text += "\nShared key: another action also uses part of this binding.";
            else if (row) text += "\nSelect a slot to rebind. Native validation decides whether a change is allowed.";
            MenuStyle.setText(detail,text);
        }
        private function selectKey(key:int):void
        {
            if (frozen || editor.busy || editor.saving) return;
            selectedKey = selectedKey == key ? -1 : key; changed();
        }
        private function nextSource(event:MouseEvent):void
        {
            if (frozen || editor.busy || editor.saving) return;
            for (var i:int = 0; i < sources.length; ++i) if (sources[i].id == source) {
                source = sources[(i + 1) % sources.length].id; changed(); return;
            }
        }
        private function clearFilters(event:MouseEvent = null):void
        {
            if (frozen || editor.busy || editor.saving) return;
            search.text = ""; source = "all"; selectedKey = -1; changed();
        }
        private function filtersChanged(event:Event):void { if (!frozen && !editor.busy && !editor.saving) changed(); }
        private function focusChanged(event:FocusEvent):void
        {
            if (!bridge.textInput(searching) && searching) {
                leaveSearch(); MenuStyle.setText(filterLabel,"Native text input is unavailable.");
            }
        }
        public function searchKey(event:KeyboardEvent):Boolean
        {
            if (!searching) return false;
            if (event.keyCode == Keyboard.ENTER) {
                event.preventDefault();
                // Finish on release, like vanilla text entry.
                if (event.type == KeyboardEvent.KEY_UP) leaveSearch();
            }
            // Text input still needs its default keyboard handling.
            return true;
        }
    }
}
