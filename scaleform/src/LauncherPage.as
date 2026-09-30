package
{
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    // Home's recent shelf and expanded, paged grid share the same selection.
    // Both layouts keep the left column so Home's detail card stays beside them.
    public final class LauncherPage extends MovieClip
    {
        // The expanded grid starts where the shelf does; its pager shares the section label's row.
        public static const GRID_TOP:Number = MenuStyle.LIST_TOP;
        private static const COLUMNS:int = 2;
        private static const SHELF_ROWS:int = 3;
        private var entries:Array = [];
        private var displayed:Array = [];
        private var cards:Sprite = new Sprite();
        private var pager:Sprite = new Sprite();
        private var changed:Function;
        private var activated:Function;
        private var layoutChanged:Function;
        private var selected:int = -1;
        private var first:int = 0;
        private var isExpanded:Boolean = false;
        private var isLocked:Boolean = false;
        private var hasFocus:Boolean = false;
        private var loadingRow:Object = null;
        public function LauncherPage(onChanged:Function, onActivated:Function, onLayout:Function)
        {
            changed = onChanged; activated = onActivated; layoutChanged = onLayout;
            x = MenuStyle.LEFT; y = MenuStyle.LIST_TOP;
            addChild(cards); addChild(pager);
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
        }
        public function get columns():int { return COLUMNS; }
        // As many shelf-height rows as fit between the grid top and the list bottom.
        private function get gridRows():int { return Math.max(1, int((MenuStyle.LIST_BOTTOM - GRID_TOP + 14) / (shelfCardHeight + 14))); }
        public function get capacity():int { return columns * (isExpanded ? gridRows : SHELF_ROWS); }
        private function get shelfCardHeight():Number { return CONFIG::largeText ? 138 : 116; }
        public function get shelfHeight():Number {
            var rows:int = Math.ceil(displayed.length / columns);
            return rows * shelfCardHeight + Math.max(0, rows - 1) * 14;
        }
        public function get onLastRow():Boolean { return selected >= int((displayed.length - 1) / columns) * columns; }
        public function get expanded():Boolean { return isExpanded; }
        // With no mod settings to show, the grid is the whole page and cannot collapse.
        public function get locked():Boolean { return isLocked; }
        public function get hasEntries():Boolean { return entries.length > 0; }
        public function get focused():Boolean { return hasFocus; }
        public function set focused(value:Boolean):void { hasFocus = value; paintSelection(); }
        public function get selectedIndex():int { return selected; }
        public function get scrollPosition():int { return first; }
        public function get current():Object { return selected >= 0 && selected < displayed.length ? displayed[selected] : null; }
        public function get count():int { return entries.length; }
        CONFIG::testHarness {
            public function get visibleCards():Array
            {
                var result:Array = [];
                for (var i:int = 0; i < cards.numChildren; ++i) result.push(cards.getChildAt(i));
                return result;
            }
        }
        public function populate(rows:Array, preserve:Boolean, lock:Boolean = false):void
        {
            var previous:Object = preserve ? current : null;
            entries = [];
            for each (var row:Object in rows) if (row.type == "launcher") {
                if (!isFinite(Number(row.recentOrder))) row.recentOrder = 0;
                entries.push(row);
            }
            entries.sortOn(["recentOrder", "modTitle", "title", "mod", "key"],
                [Array.NUMERIC | Array.DESCENDING, Array.CASEINSENSITIVE, Array.CASEINSENSITIVE, 0, 0]);
            if (!preserve || isLocked && !lock) { isExpanded = false; hasFocus = false; }
            isLocked = lock;
            if (isLocked) isExpanded = true;
            else if (entries.length <= columns * SHELF_ROWS) isExpanded = false;
            displayed = isExpanded ? entries.concat() : entries.slice(0, entries.length > capacity ? capacity - 1 : capacity);
            if (!isExpanded && entries.length > capacity) {
                displayed.push({type:"launcherMore", more:true, editable:true, mod:"", key:"@more",
                    title:tr("home.showAll") + " " + entries.length, hint:tr("home.browseAll", {count:entries.length}),
                    badge:"+" + (entries.length - displayed.length)});
            }
            selected = displayed.length ? 0 : -1;
            for (var i:int = 0; previous && i < displayed.length; ++i)
                if (displayed[i].mod == previous.mod && displayed[i].key == previous.key) selected = i;
            first = selected < 0 ? 0 : int(selected / capacity) * capacity;
            render();
        }
        public function toggleExpanded():void { if (isLocked) return; isExpanded = !isExpanded; hasFocus = true; layoutChanged(); }
        // The destination loading before handoff, matched by identity so its tag survives refreshes.
        public function set loading(row:Object):void { loadingRow = row; render(); }
        private function paintSelection():void
        {
            for (var i:int = 0; i < cards.numChildren; ++i) {
                var card:LauncherCard = cards.getChildAt(i) as LauncherCard;
                card.select(hasFocus && card.index == selected);
            }
        }
        private function select(index:int):void
        {
            selected = displayed.length ? Math.max(0, Math.min(index, displayed.length - 1)) : -1;
            var page:int = selected < 0 ? 0 : int(selected / capacity) * capacity;
            if (page != first) { first = page; render(); }
            else paintSelection();
            changed(); // Hovering the same card can still change focus from the mod list.
        }
        public function navigate(direction:String):void
        {
            if (!isExpanded && direction == "PageDown" && entries.length > displayed.length) {
                toggleExpanded(); return;
            }
            if (!displayed.length) return;
            var next:int = Math.max(0, selected);
            var across:int = columns;
            if (direction == "Left") { if (next % across > 0) --next; }
            else if (direction == "Right") { if (next % across < across - 1) ++next; }
            else if (direction == "Up") { if (next >= across) next -= across; }
            else if (direction == "Down") { if (int(next / across) < int((displayed.length - 1) / across)) next = Math.min(next + across, displayed.length - 1); }
            else if (direction == "PageUp") next -= capacity;
            else if (direction == "PageDown") next += capacity;
            select(next);
        }
        private function turnPage(direction:int):void
        {
            var page:int = first + direction * capacity;
            if (page >= 0 && page < displayed.length) select(Math.min(page + selected - first, displayed.length - 1));
        }
        private function wheel(event:MouseEvent):void
        {
            if (isExpanded && event.delta) turnPage(event.delta < 0 ? 1 : -1);
            event.stopPropagation();
        }
        private function over(event:MouseEvent):void { select(LauncherCard(event.currentTarget).index); }
        private function press(event:MouseEvent):void { select(LauncherCard(event.currentTarget).index); activated(); }
        // Returns the button's left edge so header controls can be laid out right to left.
        private function pageButton(text:String, right:Number, callback:Function, enabled:Boolean = true, buttonName:String = ""):Number
        {
            var label:TextField = MenuStyle.field(text, 4, 0, 300, MenuStyle.SMALL_SIZE + 14, MenuStyle.SMALL_SIZE + 2, MenuStyle.WHITE, true);
            label.width = Math.min(300, label.textWidth + 8); MenuStyle.fit(label, text);
            var button:Sprite = new Sprite(); button.name = buttonName; button.x = right - label.width - 8; button.y = MenuStyle.SECTION_TOP - GRID_TOP + 2;
            button.mouseChildren = false; button.buttonMode = enabled; button.alpha = enabled ? 1 : 0.35;
            button.graphics.beginFill(0, 0); button.graphics.drawRect(0, 0, label.width + 8, MenuStyle.SMALL_SIZE + 14); button.graphics.endFill();
            button.addChild(label);
            if (enabled) button.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { callback(); });
            pager.addChild(button); return button.x;
        }
        private function render():void
        {
            while (cards.numChildren) cards.removeChildAt(0);
            while (pager.numChildren) pager.removeChildAt(0);
            var gap:Number = 16, rowGap:Number = 14, across:int = columns;
            var width:Number = (MenuStyle.LIST_WIDTH - gap * (across - 1)) / across;
            // The grid runs from GRID_TOP to just above the footer divider.
            var height:Number = isExpanded ? (MenuStyle.LIST_BOTTOM - GRID_TOP - rowGap * (gridRows - 1)) / gridRows : shelfCardHeight;
            for (var i:int = first; i < Math.min(displayed.length, first + capacity); ++i) {
                displayed[i].loading = Boolean(loadingRow) && displayed[i].mod == loadingRow.mod && displayed[i].key == loadingRow.key;
                var card:LauncherCard = new LauncherCard(displayed[i], i, width, height);
                card.x = (i - first) % across * (width + gap);
                card.y = int((i - first) / across) * (height + rowGap);
                card.select(hasFocus && i == selected);
                card.addEventListener(MouseEvent.ROLL_OVER, over);
                card.addEventListener(MouseEvent.CLICK, press); cards.addChild(card);
            }
            if (!isExpanded) return;
            // Header row, right to left: SHOW LESS, then the pager when the grid has pages.
            var right:Number = isLocked ? MenuStyle.LIST_WIDTH : pageButton(tr("home.showLess"), MenuStyle.LIST_WIDTH, toggleExpanded) - 28;
            if (displayed.length > capacity) {
                right = pageButton(">", right, function():void { turnPage(1); }, first + capacity < displayed.length);
                var position:TextField = MenuStyle.field((int(first / capacity) + 1) + " / " + Math.ceil(displayed.length / capacity),
                    0, MenuStyle.SECTION_TOP - GRID_TOP + 2, 120, MenuStyle.SMALL_SIZE + 14, MenuStyle.SMALL_SIZE + 2, MenuStyle.MUTED, true);
                position.width = position.textWidth + 8; position.x = right - position.width - 4; pager.addChild(position);
                pageButton("<", position.x - 4, function():void { turnPage(-1); }, first > 0);
            }
        }
    }
}
