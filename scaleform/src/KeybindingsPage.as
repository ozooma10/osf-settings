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
        public static const KEYBOARD_TOP:Number = MenuStyle.SECTION_TOP;
        // Column labels sit under the keyboard; the results list starts under them.
        private static const COLUMNS_TOP:Number = KEYBOARD_TOP + KeyboardMap.HEIGHT + 14;
        public static const LIST_TOP:Number = COLUMNS_TOP + MenuStyle.SMALL_SIZE + 14;
        private static const BOX:Number = MenuStyle.VALUE_SIZE + 22;
        public var rows:Array = [];
        public var state:String = "loading";
        public var selectedKey:int = -1;
        public var source:String = "all";
        public var search:TextField;
        private var keyboard:KeyboardMap;
        private var sourceLabel:TextField;
        private var placeholder:TextField;
        private var keyChip:Sprite;
        private var keyLabel:TextField;
        private var clear:Sprite;
        private var clearLabel:TextField;
        private var notice:TextField;
        private var textUnavailable:Boolean;
        private var selection:Object;
        private var bridge:Object;
        private var editor:NativeHotkeysList;
        private var changed:Function;
        private var leaveSearch:Function;
        private var definitions:Array = [];
        private var sources:Array = [{id:"all",title:tr("bindings.allSources")},{id:"game",title:tr("bindings.game")}];
        private var generation:uint;
        private var revision:uint;
        private var requestedAt:int;
        private var nextPoll:int;
        private var frozen:Boolean;
        private var filteredSource:Array;
        private var filteredRows:Array;
        private var filteredQuery:String;
        private var filteredOwner:String;
        private var filteredKey:int;

        public function KeybindingsPage(code:Object, nativeEditor:NativeHotkeysList, notify:Function, resultsFocus:Function)
        {
            bridge = code; editor = nativeEditor; changed = notify; leaveSearch = resultsFocus;
            keyboard = new KeyboardMap(selectKey); keyboard.x = MenuStyle.LEFT; keyboard.y = KEYBOARD_TOP; addChild(keyboard);
            // Filters share the right column so the results list can start under the keyboard.
            var left:Number = MenuStyle.DETAIL_X, column:Number = MenuStyle.DETAIL_WIDTH, top:Number = LIST_TOP;
            var size:Number = MenuStyle.VALUE_SIZE + 2, line:Number = size + 12, inset:Number = (BOX - line) / 2 + 2;
            placeholder = field(tr("bindings.searchHint"),left + 12,top + inset,column - 24,line,size,MenuStyle.MUTED);
            search = field("",left + 12,top + inset,column - 24,line,size);
            nameField(search,"bindingSearch"); search.type = "input"; search.selectable = true; search.mouseEnabled = true;
            search.maxChars = 128;
            search.addEventListener(Event.CHANGE,filtersChanged);
            search.addEventListener(FocusEvent.FOCUS_IN,focusChanged);
            search.addEventListener(FocusEvent.FOCUS_OUT,focusChanged);
            sourceLabel = field("",left + 12,top + BOX + 12 + inset,column - 24,line,MenuStyle.VALUE_SIZE);
            nameField(sourceLabel,"bindingSource").addEventListener(MouseEvent.CLICK,nextSource); sourceLabel.mouseEnabled = true;
            keyLabel = field("",left + 12,top + (BOX + 12) * 2 + inset,400,line,MenuStyle.VALUE_SIZE);
            keyChip = nameField(keyLabel,"bindingKeyFilter"); keyChip.addEventListener(MouseEvent.CLICK,clearKey);
            keyLabel.mouseEnabled = true;
            clearLabel = field(tr("bindings.clearFilters"),MenuStyle.RIGHT - 300,top + (BOX + 12) * 2 + inset,300,line,MenuStyle.VALUE_SIZE,MenuStyle.MUTED);
            clearLabel.width = Math.min(364,clearLabel.textWidth + 8); clearLabel.x = MenuStyle.RIGHT - clearLabel.width;
            clear = nameField(clearLabel,"clearBindingFilters"); clear.addEventListener(MouseEvent.CLICK,clearFilters);
            clearLabel.mouseEnabled = true;
            notice = field("",left,0,column,150,MenuStyle.SMALL_SIZE,MenuStyle.MUTED);
            notice.multiline = true; notice.wordWrap = true;
            // Labels sit over the native binding cells, which are anchored to the row's right edge.
            field(tr("bindings.primary"),MenuStyle.LEFT + MenuStyle.LIST_WIDTH - 450,COLUMNS_TOP,210,MenuStyle.SMALL_SIZE + 12,MenuStyle.SMALL_SIZE,MenuStyle.MUTED);
            field(tr("bindings.alternate"),MenuStyle.LEFT + MenuStyle.LIST_WIDTH - 212,COLUMNS_TOP,210,MenuStyle.SMALL_SIZE + 12,MenuStyle.SMALL_SIZE,MenuStyle.MUTED);
            graphics.lineStyle(1,MenuStyle.LINE); graphics.drawRect(left,top,column,BOX);
            graphics.drawRect(left,top + BOX + 12,column,BOX);
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
            definitions = metadata; visible = true; textUnavailable = false; rows = []; editor.open(); request();
        }
        public function close():void
        {
            if (searching) leaveSearch();
            visible = false; ++generation;
        }
        private function request():void
        {
            // Keep the published rows visible until their replacement is ready.
            revision = editor.revision; state = "loading";
            generation = bridge.requestBindings(); requestedAt = getTimer(); nextPoll = 0;
        }
        public function advance(metadata:Array, busy:Boolean):void
        {
            if (!visible) return;
            frozen = busy; mouseChildren = !busy;
            if (busy) return;
            definitions = metadata;
            if (revision != editor.revision) request();
            if (state != "loading" || getTimer() < nextPoll) return;
            nextPoll = getTimer() + 100;
            var snapshot:Object = bridge.pollBindings();
            if (snapshot && snapshot.generation == generation && snapshot.state != "loading") {
                if (snapshot.state == "ready" && editor.ready) {
                    rows = KeybindingsData.join(editor.bindings,definitions,snapshot.records,editor.title);
                    state = "ready"; updateSources(); changed(); return;
                }
                if (snapshot.state == "unavailable") { rows = []; state = "unavailable"; changed(); return; }
            }
            if (getTimer() - requestedAt > 5000) { rows = []; state = "unavailable"; changed(); }
        }
        private function updateSources():void
        {
            sources = [{id:"all",title:tr("bindings.allSources")},{id:"game",title:tr("bindings.game")}];
            var seen:Object = {};
            for each (var row:Object in rows) if (row.mod && !seen[row.mod]) {
                sources.push({id:row.mod,title:row.source}); seen[row.mod] = true;
            }
            if (source != "all" && source != "game" && !seen[source]) source = "all";
        }
        public function filtered():Array
        {
            // Published snapshots replace rows; selection changes do not invalidate a filter.
            if (filteredSource != rows || filteredQuery != search.text || filteredOwner != source || filteredKey != selectedKey) {
                filteredSource = rows; filteredQuery = search.text; filteredOwner = source; filteredKey = selectedKey;
                filteredRows = KeybindingsData.filter(rows, filteredQuery, source, selectedKey);
            }
            return filteredRows;
        }
        public function get emptyText():String
        {
            return state == "loading" ? tr("bindings.loading") : state == "unavailable" ?
                tr("bindings.retry") : tr("bindings.noMatches");
        }
        public function showSelection(row:Object):void
        {
            selection = row;
            keyboard.update(rows,filtered(),row,selectedKey);
            var title:String = tr("bindings.allSources");
            for each (var choice:Object in sources) if (choice.id == source) title = choice.title;
            MenuStyle.fit(sourceLabel,tr("bindings.source", {source:title}));
            placeholder.visible = !search.text && !searching;
            keyChip.visible = selectedKey >= 0;
            if (keyChip.visible) {
                keyLabel.width = 400; MenuStyle.fit(keyLabel,tr("bindings.keyFilter", {key:KeyboardMap.keyName(selectedKey,0)}));
                keyLabel.width = Math.min(400,keyLabel.textWidth + 8);
                keyChip.graphics.clear(); keyChip.graphics.lineStyle(2,MenuStyle.WHITE); keyChip.graphics.beginFill(MenuStyle.INK,0);
                keyChip.graphics.drawRect(MenuStyle.DETAIL_X,LIST_TOP + (BOX + 12) * 2,keyLabel.width + 24,BOX); keyChip.graphics.endFill();
            }
            clear.visible = Boolean(search.text) || source != "all" || selectedKey >= 0;
            notice.y = LIST_TOP + (BOX + 12) * (keyChip.visible || clear.visible ? 3 : 2) + 6;
            showNotice();
        }
        private function showNotice():void
        {
            // Only flags the list row cannot show; everything else is already on the row.
            var flags:Array = [];
            if (selection && !selection.available) flags.push(tr("bindings.unavailable"));
            if (selection && selection.binding.bRequired) flags.push(tr("bindings.required"));
            if (selection && selection.binding.bReadOnly) flags.push(tr("bindings.readOnly"));
            var text:String = flags.length ? selection.title + "  |  " + flags.join("  |  ") : "";
            if (textUnavailable) text = tr("bindings.textUnavailable") + (text ? "\n" + text : "");
            MenuStyle.setText(notice,text);
        }
        private function selectKey(key:int):void
        {
            if (frozen || editor.busy || editor.saving) return;
            selectedKey = selectedKey == key ? -1 : key; changed();
        }
        private function clearKey(event:MouseEvent):void
        {
            if (frozen || editor.busy || editor.saving) return;
            selectedKey = -1; changed();
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
            var active:Boolean = searching;
            if (!bridge.textInput(active) && active) { textUnavailable = true; leaveSearch(); }
            else if (active) textUnavailable = false;
            placeholder.visible = !search.text && !searching;
            showNotice();
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
