package
{
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    // Home's recent shelf and expanded, paged grid share the same selection.
    public final class LauncherPage extends MovieClip
    {
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
        private var hasFocus:Boolean = false;
        public function LauncherPage(onChanged:Function, onActivated:Function, onLayout:Function)
        {
            changed = onChanged; activated = onActivated; layoutChanged = onLayout;
            x = MenuStyle.LEFT; y = MenuStyle.LIST_TOP;
            addChild(cards); addChild(pager);
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
        }
        public function get columns():int { return CONFIG::largeText ? 4 : 5; }
        public function get capacity():int { return columns * (isExpanded ? 2 : 1); }
        public function get shelfHeight():Number { return CONFIG::largeText ? 260 : 220; }
        public function get expanded():Boolean { return isExpanded; }
        public function get hasEntries():Boolean { return entries.length > 0; }
        public function get focused():Boolean { return hasFocus; }
        public function set focused(value:Boolean):void { hasFocus = value; paintSelection(); }
        public function get selectedIndex():int { return selected; }
        public function get scrollPosition():int { return first; }
        public function get current():Object { return selected >= 0 && selected < displayed.length ? displayed[selected] : null; }
        public function get countText():String { return tr("home.interfaceCount", {count:entries.length}); }
        CONFIG::testHarness {
            public function get visibleCards():Array
            {
                var result:Array = [];
                for (var i:int = 0; i < cards.numChildren; ++i) result.push(cards.getChildAt(i));
                return result;
            }
        }
        public function populate(rows:Array, preserve:Boolean):void
        {
            var previous:Object = preserve ? current : null;
            entries = [];
            for each (var row:Object in rows) if (row.type == "launcher") {
                if (!isFinite(Number(row.recentOrder))) row.recentOrder = 0;
                entries.push(row);
            }
            entries.sortOn(["recentOrder", "modTitle", "title", "mod", "key"],
                [Array.NUMERIC | Array.DESCENDING, Array.CASEINSENSITIVE, Array.CASEINSENSITIVE, 0, 0]);
            if (!preserve) { isExpanded = false; hasFocus = false; }
            if (entries.length <= columns) isExpanded = false;
            displayed = isExpanded || entries.length <= columns ? entries.concat() : entries.slice(0, columns - 1);
            if (!isExpanded && entries.length > columns) {
                var remaining:int = entries.length - displayed.length;
                displayed.push({type:"launcherMore", more:true, editable:true, mod:"", key:"@more",
                    title:tr("home.showMore", {count:remaining}), hint:tr("home.browseAll", {count:entries.length}), badge:"+" + remaining});
            }
            selected = displayed.length ? 0 : -1;
            for (var i:int = 0; previous && i < displayed.length; ++i)
                if (displayed[i].mod == previous.mod && displayed[i].key == previous.key) selected = i;
            first = selected < 0 ? 0 : int(selected / capacity) * capacity;
            render();
        }
        public function toggleExpanded():void { isExpanded = !isExpanded; hasFocus = true; layoutChanged(); }
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
            if (!displayed.length) return;
            var next:int = Math.max(0, selected);
            if (direction == "Left") { if (next % columns > 0) --next; }
            else if (direction == "Right") { if (next % columns < columns - 1) ++next; }
            else if (direction == "Up") { if (next >= columns) next -= columns; }
            else if (direction == "Down") { if (int(next / columns) < int((displayed.length - 1) / columns)) next = Math.min(next + columns, displayed.length - 1); }
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
        private function pageButton(text:String, x:Number, y:Number, callback:Function, enabled:Boolean = true):void
        {
            var button:Sprite = new Sprite(); button.x = x; button.y = y;
            button.mouseChildren = false; button.buttonMode = enabled; button.alpha = enabled ? 1 : 0.35;
            button.graphics.beginFill(0, 0); button.graphics.drawRect(0, 0, 220, 36); button.graphics.endFill();
            MenuStyle.fit(TextField(button.addChild(MenuStyle.field(text, 4, 0, 212, 36, 24, MenuStyle.WHITE, true))), text);
            if (enabled) button.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { callback(); });
            pager.addChild(button);
        }
        private function render():void
        {
            while (cards.numChildren) cards.removeChildAt(0);
            while (pager.numChildren) pager.removeChildAt(0);
            var gap:Number = 20;
            var width:Number = (MenuStyle.RIGHT - MenuStyle.LEFT - gap * (columns - 1)) / columns;
            var height:Number = isExpanded ? (MenuStyle.LIST_HEIGHT - gap) / 2 : shelfHeight;
            for (var i:int = first; i < Math.min(displayed.length, first + capacity); ++i) {
                var card:LauncherCard = new LauncherCard(displayed[i], i, width, height);
                card.x = (i - first) % columns * (width + gap);
                card.y = int((i - first) / columns) * (height + gap);
                card.select(hasFocus && i == selected);
                card.addEventListener(MouseEvent.ROLL_OVER, over);
                card.addEventListener(MouseEvent.CLICK, press); cards.addChild(card);
            }
            if (isExpanded) pageButton(tr("home.showLess"), MenuStyle.RIGHT - MenuStyle.LEFT - 212, -57, toggleExpanded);
            if (isExpanded && displayed.length > capacity) {
                pageButton("< " + tr("buttons.previousPage"), 500, MenuStyle.LIST_HEIGHT + 10, function():void { turnPage(-1); }, first > 0);
                pager.addChild(MenuStyle.field((int(first / capacity) + 1) + " / " + Math.ceil(displayed.length / capacity), 818,
                    MenuStyle.LIST_HEIGHT + 10, 170, 32, 24, MenuStyle.MUTED, true));
                pageButton(tr("buttons.nextPage") + " >", 986, MenuStyle.LIST_HEIGHT + 10, function():void { turnPage(1); }, first + capacity < displayed.length);
            }
        }
    }
}
