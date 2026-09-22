package
{
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    public final class LauncherPage extends MovieClip
    {
        private var entries:Array = [];
        private var cards:Sprite = new Sprite();
        private var pager:Sprite = new Sprite();
        private var changed:Function;
        private var activated:Function;
        private var selected:int = -1;
        private var first:int = 0;
        public function LauncherPage(onChanged:Function, onActivated:Function)
        {
            changed = onChanged; activated = onActivated;
            x = MenuStyle.LEFT; y = MenuStyle.LIST_TOP;
            addChild(cards); addChild(pager);
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
        }
        public function get columns():int { return CONFIG::largeText ? 3 : 4; }
        public function get capacity():int { return columns; }
        public function get selectedIndex():int { return selected; }
        public function get scrollPosition():int { return first; }
        CONFIG::testHarness {
            public function get visibleCards():Array
            {
                var result:Array = [];
                for (var i:int = 0; i < cards.numChildren; ++i) result.push(cards.getChildAt(i));
                return result;
            }
        }
        public function get current():Object { return selected >= 0 && selected < entries.length ? entries[selected] : null; }
        public function get countText():String
        {
            return entries.length ? (first + 1) + "-" + Math.min(entries.length, first + capacity) + " / " + entries.length : "0 INTERFACES";
        }
        public function populate(rows:Array, preserve:Boolean):void
        {
            var previous:Object = preserve ? current : null;
            var index:int = preserve ? selected : 0;
            entries = [];
            for each (var row:Object in rows) if (row.type == "launcher") entries.push(row);
            entries.sortOn(["modTitle", "title", "mod", "key"], [Array.CASEINSENSITIVE, Array.CASEINSENSITIVE, 0, 0]);
            for (var i:int = 0; previous && i < entries.length; ++i)
                if (entries[i].mod == previous.mod && entries[i].key == previous.key) index = i;
            selected = entries.length ? Math.max(0, Math.min(index, entries.length - 1)) : -1;
            first = selected < 0 ? 0 : int(selected / capacity) * capacity;
            render();
        }
        private function select(index:int):void
        {
            index = entries.length ? Math.max(0, Math.min(index, entries.length - 1)) : -1;
            if (index == selected) return;
            selected = index;
            var page:int = selected < 0 ? 0 : int(selected / capacity) * capacity;
            if (page != first) { first = page; render(); }
            else for (var i:int = 0; i < cards.numChildren; ++i) {
                var card:LauncherCard = cards.getChildAt(i) as LauncherCard;
                card.select(card.index == selected);
            }
            changed();
        }
        public function navigate(direction:String):void
        {
            if (!entries.length) return;
            var next:int = selected < 0 ? 0 : selected;
            if (direction == "Left") { if (next % columns > 0) --next; }
            else if (direction == "Right") { if (next % columns < columns - 1) ++next; }
            else if (direction == "Up") { if (next >= columns) next -= columns; }
            else if (direction == "Down") { if (int(next / columns) < int((entries.length - 1) / columns)) next = Math.min(next + columns, entries.length - 1); }
            else if (direction == "PageUp") next -= capacity;
            else if (direction == "PageDown") next += capacity;
            select(next);
        }
        private function turnPage(direction:int):void
        {
            var page:int = first + direction * capacity;
            if (page >= 0 && page < entries.length) select(Math.min(page + selected - first, entries.length - 1));
        }
        private function wheel(event:MouseEvent):void
        {
            if (event.delta) turnPage(event.delta < 0 ? 1 : -1);
            event.stopPropagation();
        }
        private function over(event:MouseEvent):void { select(LauncherCard(event.currentTarget).index); }
        private function press(event:MouseEvent):void
        {
            select(LauncherCard(event.currentTarget).index); activated();
        }
        private function pageButton(text:String, x:Number, direction:int, enabled:Boolean):void
        {
            var button:Sprite = new Sprite(); button.x = x; button.y = MenuStyle.LIST_HEIGHT + 10;
            button.mouseChildren = false; button.buttonMode = enabled; button.alpha = enabled ? 1 : 0.35;
            button.graphics.beginFill(0, 0); button.graphics.drawRect(0, 0, 150, 32); button.graphics.endFill();
            button.addChild(MenuStyle.field(text, 4, 0, 145, 32, 24, MenuStyle.WHITE, true));
            if (enabled) button.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { turnPage(direction); });
            pager.addChild(button);
        }
        private function render():void
        {
            while (cards.numChildren) cards.removeChildAt(0);
            while (pager.numChildren) pager.removeChildAt(0);
            var gap:Number = 20;
            var width:Number = (MenuStyle.RIGHT - MenuStyle.LEFT - gap * (columns - 1)) / columns;
            var height:Number = MenuStyle.LIST_HEIGHT;
            for (var i:int = first; i < Math.min(entries.length, first + capacity); ++i) {
                var card:LauncherCard = new LauncherCard(entries[i], i, width, height);
                card.x = (i - first) % columns * (width + gap);
                card.y = int((i - first) / columns) * (height + gap);
                card.select(i == selected);
                card.addEventListener(MouseEvent.ROLL_OVER, over);
                card.addEventListener(MouseEvent.CLICK, press); cards.addChild(card);
            }
            if (entries.length > capacity) {
                pageButton("< PREV", 642, -1, first > 0);
                var counter:TextField = MenuStyle.field((int(first / capacity) + 1) + " / " + Math.ceil(entries.length / capacity), 818,
                    MenuStyle.LIST_HEIGHT + 10, 170, 32, 24, MenuStyle.MUTED, true);
                pager.addChild(counter);
                pageButton("NEXT >", 986, 1, first + capacity < entries.length);
            }
        }
    }
}
