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
        private static const PANEL:uint = 0x0A1920;
        private static const PANEL_HOVER:uint = 0x182A33;
        private var items:Array = [];
        private var views:Array = [];
        private var viewport:Sprite = new Sprite();
        private var content:Sprite = new Sprite();
        private var cursor:int = -1;
        private var hovered:Sprite;
        private var hasFocus:Boolean = false;
        private var signature:String = "";
        private var scroll:Number = 0;
        private var extent:Number = 0;
        private var chosen:Function;
        // The row a press already opened, so the click that follows it does not open it again.
        private var pressedRow:Object;

        public function NavigationPane(clicked:Function)
        {
            chosen = clicked; name = "navigation";
            graphics.beginFill(PANEL); graphics.drawRect(0, 0, MenuStyle.NAV_WIDTH, 1080); graphics.endFill();
            graphics.lineStyle(1, 0x1F323B); graphics.moveTo(MenuStyle.NAV_WIDTH, 0); graphics.lineTo(MenuStyle.NAV_WIDTH, 1080);
            var title:TextField = MenuStyle.field(tr("menu.title"), PAD, MenuStyle.TITLE_TOP,
                ITEM_WIDTH, MenuStyle.TITLE_SIZE + 16, MenuStyle.TITLE_SIZE, MenuStyle.MUTED, true);
            MenuStyle.fit(title, tr("menu.title"));
            addChild(title);
            MenuDecoration.stripe(graphics, PAD, MenuStyle.HEADER_LINE, ITEM_WIDTH);
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
            views = []; hovered = null; var top:Number = 0;
            for (var i:int = 0; i < items.length; ++i) {
                var item:Object = items[i];
                var heading:Boolean = item.kind == "heading", nested:Boolean = item.kind == "section";
                var span:Number = heading ? HEADING : ROW;
                var row:Sprite = new Sprite(); row.name = item.name || key(item); row.y = top; row.x = PAD;
                row.mouseChildren = false; row.buttonMode = !heading;
                var left:Number = nested ? 40 : 14;
                var size:Number = heading ? MenuStyle.SMALL_SIZE : nested ? MenuStyle.SMALL_SIZE + 2 : MenuStyle.BODY_SIZE - 1;
                var label:TextField = MenuStyle.field("", left, 0, ITEM_WIDTH - left - 70, size + 12, size, MenuStyle.WHITE, heading);
                // Center on the font size as SettingsRow does; the field box includes descender space caps never use.
                label.y = heading ? span - label.height - 2 : (span - size) / 2 - 3;
                MenuStyle.fit(label, heading ? String(item.title).toUpperCase() : String(item.title));
                var count:TextField = MenuStyle.field("", ITEM_WIDTH - 80, 0, 66, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE - 1, MenuStyle.MUTED, true);
                var format:TextFormat = count.defaultTextFormat; format.align = "right"; count.defaultTextFormat = format;
                count.y = (span - MenuStyle.SMALL_SIZE + 1) / 2 - 3; MenuStyle.setText(count, item.count ? String(item.count) : "");
                if (heading) { count.x = label.x + label.textWidth + 12; count.y = label.y + 1; format.align = "left"; count.defaultTextFormat = format; MenuStyle.setText(count, count.text); }
                row.addChild(label); row.addChild(count);
                // Rows open on press: the game's player drops the click when a row repaints under it.
                if (!heading) {
                    row.addEventListener(MouseEvent.MOUSE_DOWN, pressed); row.addEventListener(MouseEvent.CLICK, clicked);
                    row.addEventListener(MouseEvent.ROLL_OVER, hover); row.addEventListener(MouseEvent.ROLL_OUT, hover);
                }
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
                var active:Boolean = i == cursor, inverted:Boolean = active && hasFocus, pointed:Boolean = row == hovered;
                if (view.active === active && view.inverted === inverted && view.pointed === pointed) continue;
                view.active = active; view.inverted = inverted; view.pointed = pointed;
                row.graphics.clear();
                if (item.kind == "heading") { view.label.textColor = view.count.textColor = MenuStyle.MUTED; continue; }
                // Idle rows fill with the panel color; the game's player skips fully transparent hit areas.
                row.graphics.beginFill(inverted ? MenuStyle.WHITE : active ? (pointed ? MenuStyle.HOVER : MenuStyle.ROW) : pointed ? PANEL_HOVER : PANEL);
                row.graphics.drawRect(0, 0, ITEM_WIDTH, view.span); row.graphics.endFill();
                view.label.textColor = inverted ? MenuStyle.INK : item.kind == "section" && !active && !pointed ? MenuStyle.MUTED : MenuStyle.WHITE;
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
        private function hover(event:MouseEvent):void
        {
            var next:Sprite = event.type == MouseEvent.ROLL_OVER ? event.currentTarget as Sprite : hovered == event.currentTarget ? null : hovered;
            if (next != hovered) { hovered = next; paint(); }
        }
        private function pressed(event:MouseEvent):void { pressedRow = event.currentTarget; open(event.currentTarget); }
        // A click without a press first (a scripted one) still opens its row.
        private function clicked(event:MouseEvent):void
        {
            if (pressedRow == event.currentTarget) { pressedRow = null; return; }
            open(event.currentTarget);
        }
        private function open(target:Object):void
        {
            for (var i:int = 0; i < views.length; ++i) if (views[i].row == target) { chosen(views[i].item); return; }
        }
    }
}
