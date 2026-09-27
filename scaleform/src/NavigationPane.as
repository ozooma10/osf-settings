package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // The menu's left column: pinned pages, then every mod, with the open mod's sections
    // nested under it. Items are plain objects the menu builds; the menu decides what
    // opening one does. Sprites are named like the tabs they replace so tests can click them.
    public final class NavigationPane extends Sprite
    {
        private static const PAD:Number = 32;
        private static const ITEM_WIDTH:Number = MenuStyle.NAV_WIDTH - PAD * 2;
        private static const TOP:Number = MenuStyle.SECTION_TOP - 6;
        private static const BOTTOM:Number = 1052;
        private static const ROW:Number = CONFIG::largeText ? 50 : 40;
        private static const HEADING:Number = CONFIG::largeText ? 56 : 48;
        private static const GAP:Number = 2;
        private var items:Array = [];
        private var views:Array = [];
        private var viewport:Sprite = new Sprite();
        private var content:Sprite = new Sprite();
        private var cursor:int = -1;
        private var hasFocus:Boolean = false;
        private var signature:String = "";
        private var scroll:Number = 0;
        private var extent:Number = 0;
        private var chosen:Function;

        public function NavigationPane(clicked:Function)
        {
            chosen = clicked; name = "navigation";
            graphics.beginFill(0x0A1920, 0.92); graphics.drawRect(0, 0, MenuStyle.NAV_WIDTH, 1080); graphics.endFill();
            graphics.lineStyle(1, 0x1F323B); graphics.moveTo(MenuStyle.NAV_WIDTH, 0); graphics.lineTo(MenuStyle.NAV_WIDTH, 1080);
            var title:TextField = MenuStyle.field(tr("menu.title"), PAD, MenuStyle.TITLE_TOP + MenuStyle.TITLE_SIZE - MenuStyle.SECTION_SIZE - 6,
                ITEM_WIDTH, MenuStyle.SECTION_SIZE + 12, MenuStyle.SECTION_SIZE, MenuStyle.MUTED, true);
            addChild(title);
            viewport.y = TOP; viewport.addChild(content); addChild(viewport);
            viewport.scrollRect = new Rectangle(0, 0, MenuStyle.NAV_WIDTH, BOTTOM - TOP);
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
        }

        public static function key(item:Object):String { return item ? item.kind + "/" + item.id : ""; }
        public function get selected():Object { return cursor >= 0 && cursor < items.length ? items[cursor] : null; }
        public function get focused():Boolean { return hasFocus; }
        public function set focused(value:Boolean):void { if (hasFocus != value) { hasFocus = value; paint(); } }

        // items: [{kind:"page"|"heading"|"mod"|"section", id, name, title, count, accent}]
        public function update(next:Array, selectedKey:String):void
        {
            var parts:Array = [];
            for each (var item:Object in next) parts.push([item.kind, item.id, item.title, item.count, item.accent].join("|"));
            var nextSignature:String = parts.join("\n");
            if (nextSignature != signature) { signature = nextSignature; items = next; build(); }
            cursor = -1;
            for (var i:int = 0; i < items.length; ++i) if (key(items[i]) == selectedKey) cursor = i;
            if (cursor < 0) cursor = step(-1, 1, false);
            paint(); reveal();
        }
        // The next selectable entry from the cursor. Top-level moves skip nested sections.
        public function step(from:int, direction:int, topLevel:Boolean):int
        {
            for (var i:int = from + direction; i >= 0 && i < items.length; i += direction) {
                var kind:String = items[i].kind;
                if (kind != "heading" && (!topLevel || kind != "section")) return i;
            }
            return -1;
        }
        public function move(direction:int, topLevel:Boolean = false):Object
        {
            var next:int = step(cursor, direction, topLevel);
            if (next < 0) return null;
            cursor = next; paint(); reveal();
            return items[cursor];
        }

        private function build():void
        {
            while (content.numChildren) content.removeChildAt(0);
            views = []; var top:Number = 0;
            for (var i:int = 0; i < items.length; ++i) {
                var item:Object = items[i];
                var heading:Boolean = item.kind == "heading", nested:Boolean = item.kind == "section";
                var span:Number = heading ? HEADING : ROW;
                var row:Sprite = new Sprite(); row.name = item.name || key(item); row.y = top; row.x = PAD;
                row.mouseChildren = false; row.buttonMode = !heading;
                var left:Number = nested ? 40 : 14;
                var size:Number = heading ? MenuStyle.SMALL_SIZE : nested ? MenuStyle.SMALL_SIZE + 2 : MenuStyle.BODY_SIZE - 1;
                var label:TextField = MenuStyle.field("", left, 0, ITEM_WIDTH - left - 70, size + 12, size, MenuStyle.WHITE, heading);
                label.y = heading ? span - label.height - 2 : (span - label.height) / 2 + 1;
                MenuStyle.fit(label, heading ? String(item.title).toUpperCase() : String(item.title));
                var count:TextField = MenuStyle.field("", ITEM_WIDTH - 80, 0, 66, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE - 1, MenuStyle.MUTED, true);
                var format:TextFormat = count.defaultTextFormat; format.align = "right"; count.defaultTextFormat = format;
                count.y = (span - count.height) / 2 + 2; MenuStyle.setText(count, item.count ? String(item.count) : "");
                if (heading) { count.x = label.x + label.textWidth + 12; count.y = label.y + 1; format.align = "left"; count.defaultTextFormat = format; MenuStyle.setText(count, count.text); }
                row.addChild(label); row.addChild(count);
                if (!heading) row.addEventListener(MouseEvent.CLICK, clicked);
                content.addChild(row);
                views.push({row:row, label:label, count:count, span:span, item:item});
                top += span + GAP;
            }
            extent = top;
            scroll = Math.max(0, Math.min(scroll, extent - (BOTTOM - TOP)));
        }
        private function paint():void
        {
            for (var i:int = 0; i < views.length; ++i) {
                var view:Object = views[i], item:Object = view.item, row:Sprite = view.row;
                row.graphics.clear();
                if (item.kind == "heading") { view.label.textColor = view.count.textColor = MenuStyle.MUTED; continue; }
                var active:Boolean = i == cursor, inverted:Boolean = active && hasFocus;
                // A transparent fill keeps the whole row clickable.
                row.graphics.beginFill(inverted ? MenuStyle.WHITE : MenuStyle.ROW, active ? 1 : 0);
                row.graphics.drawRect(0, 0, ITEM_WIDTH, view.span); row.graphics.endFill();
                view.label.textColor = inverted ? MenuStyle.INK : item.kind == "section" && !active ? MenuStyle.MUTED : MenuStyle.WHITE;
                view.count.textColor = inverted ? 0x3D4F58 : item.accent ? MenuStyle.ACCENT : MenuStyle.MUTED;
            }
        }
        // Keep the cursor inside the scrolled viewport.
        private function reveal():void
        {
            if (cursor < 0 || cursor >= views.length) return;
            var view:Object = views[cursor], span:Number = BOTTOM - TOP;
            if (view.row.y < scroll) scroll = view.row.y;
            else if (view.row.y + view.span > scroll + span) scroll = view.row.y + view.span - span;
            viewport.scrollRect = new Rectangle(0, scroll, MenuStyle.NAV_WIDTH, span);
        }
        private function wheel(event:MouseEvent):void
        {
            var span:Number = BOTTOM - TOP;
            scroll = Math.max(0, Math.min(scroll - event.delta * ROW, Math.max(0, extent - span)));
            viewport.scrollRect = new Rectangle(0, scroll, MenuStyle.NAV_WIDTH, span);
            event.stopPropagation();
        }
        private function clicked(event:MouseEvent):void
        {
            for (var i:int = 0; i < views.length; ++i) if (views[i].row == event.currentTarget) { chosen(views[i].item); return; }
        }
    }
}
