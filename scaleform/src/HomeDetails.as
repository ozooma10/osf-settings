package
{
    import flash.display.Sprite;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // Home's right-hand card describes the selected mod, interface or SHOW ALL entry.
    public final class HomeDetails extends Sprite
    {
        private static const WIDTH:Number = MenuStyle.DETAIL_WIDTH;
        private static const BOTTOM:Number = MenuStyle.LIST_BOTTOM;
        private static const PAD:Number = 22;
        private static const BADGE:Number = CONFIG::largeText ? 64 : 56;
        private var content:Sprite = new Sprite();
        private var signature:String = "";
        private var selected:Object;

        public function HomeDetails()
        {
            name = "homeDetails"; x = MenuStyle.DETAIL_X; mouseEnabled = false; addChild(content);
        }

        public function get detail():Object { return selected; }

        public function show(row:Object, top:Number):void
        {
            var detail:Object = describe(row);
            var next:String = detail ? [top, detail.title, detail.badge, detail.tint, detail.subtitle, detail.description, detail.warning,
                detail.chips.join("|"), keys(detail.hotkeys)].join("\n") : "";
            if (next == signature) return;
            signature = next; selected = detail;
            while (content.numChildren) content.removeChildAt(0);
            graphics.clear();
            if (!detail) return;
            y = top;
            var height:Number = BOTTOM - top;
            graphics.lineStyle(1, 0x34424B); graphics.beginFill(0x0E1B23);
            graphics.drawRect(0, 0, WIDTH, height); graphics.endFill();
            MenuDecoration.corners(graphics, 4, 4, WIDTH - 8, height - 8);
            var large:Boolean = CONFIG::largeText;
            var badge:Badge = new Badge(detail.badge, BADGE); badge.paint(detail.tint);
            badge.x = PAD; badge.y = PAD; content.addChild(badge);
            var title:TextField = text(detail.title, PAD + BADGE + 18, PAD, WIDTH - PAD * 2 - BADGE - 18, MenuStyle.DETAIL_TITLE_SIZE, MenuStyle.WHITE, false, 2);
            title.y = PAD + Math.max(0, (BADGE - title.height) / 2);
            var cursor:Number = Math.max(PAD + BADGE, title.y + title.height) + 16;
            if (detail.subtitle) cursor = text(String(detail.subtitle).toUpperCase(), PAD, cursor, WIDTH - PAD * 2, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true, 1).y + 30;

            // Content flows top-down. Hotkeys get the room left after the chips and, when there
            // is one, a line of description; the description takes the rest.
            var limit:Number = height - PAD;
            var chipBlock:Number = detail.chips.length ? 44 : 0;
            var hotkeys:Array = detail.hotkeys || [];
            var rowHeight:Number = large ? 42 : 36;
            var reserve:Number = detail.warning || detail.description ? 50 : 0;
            var room:int = hotkeys.length ? Math.max(0, Math.floor((limit - cursor - reserve - chipBlock - 17) / rowHeight)) : 0;
            var shown:int = hotkeys.length > room ? Math.max(0, room - 1) : hotkeys.length;
            var more:Boolean = shown < hotkeys.length && room > 0;
            var hotkeyBlock:Number = shown || more ? 17 + (shown + (more ? 1 : 0)) * rowHeight : 0;
            var summary:String = detail.warning || detail.description;
            var space:Number = limit - cursor - chipBlock - hotkeyBlock;
            if (summary && space > 30) {
                var body:TextField = text(summary, PAD, cursor, WIDTH - PAD * 2, MenuStyle.DETAIL_BODY_SIZE,
                    detail.warning ? MenuStyle.ACCENT : MenuStyle.MUTED, false, 0, space - 16);
                cursor = body.y + body.height + 12;
            }
            var chipX:Number = PAD;
            for each (var chip:String in detail.chips) chipX = tag(chip, chipX, cursor) + 10;
            cursor += chipBlock;
            if (shown || more) {
                graphics.lineStyle(1, 0x34424B); graphics.moveTo(PAD, cursor); graphics.lineTo(WIDTH - PAD, cursor); graphics.lineStyle();
                var rowTop:Number = cursor + 16;
                for (var i:int = 0; i < shown; ++i, rowTop += rowHeight) {
                    var key:TextField = keycap(String(hotkeys[i].key), rowTop);
                    text(hotkeys[i].title, PAD, rowTop + 2, key.x - PAD - 30, MenuStyle.BODY_SIZE - 1, MenuStyle.WHITE, false, 1);
                }
                if (more) text(tr("home.moreHotkeys", {count:hotkeys.length - shown}), PAD, rowTop + 4, WIDTH - PAD * 2, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true, 1);
            }
            MenuDecoration.orbit(graphics, PAD, cursor + hotkeyBlock + 20, WIDTH - PAD * 2, height - PAD);
        }

        private function describe(row:Object):Object
        {
            if (!row) return null;
            if (row.more) return {title:row.title, badge:row.badge, tint:Badge.MORE, subtitle:"", description:row.hint,
                warning:"", chips:[], hotkeys:[]};
            if (row.type == "launcher") return {title:row.title, badge:Badge.initials(String(row.title)),
                tint:row.editable ? Badge.color(row.mod + "/" + row.key) : MenuStyle.LINE,
                subtitle:row.modTitle && row.modTitle != row.title ? row.modTitle : "", description:row.hint,
                warning:row.editable ? "" : String(row.message || tr("home.unavailable")),
                chips:row.editable ? [tr("home.interface")] : [tr("home.interface"), tr("home.unavailable")], hotkeys:[]};
            var hotkeys:Array = [];
            for each (var hotkey:Object in row.hotkeys) hotkeys.push({title:hotkey.title, key:hotkey.value || tr("values.unboundTitle")});
            return {title:row.title, badge:Badge.initials(String(row.title)), tint:Badge.color(String(row.mod)), subtitle:"",
                description:row.hint, warning:"", chips:row.chips, hotkeys:hotkeys};
        }

        private function keys(hotkeys:Array):String
        {
            var result:Array = [];
            for each (var hotkey:Object in hotkeys || []) result.push(hotkey.title + "=" + hotkey.key);
            return result.join(",");
        }

        // lines: 0 wraps freely within maxHeight; otherwise truncates to that many lines.
        private function text(value:String, left:Number, top:Number, width:Number, size:Number, color:uint,
                              label:Boolean, lines:int, maxHeight:Number = 0):TextField
        {
            var field:TextField = MenuStyle.field("", left, top, width, 40, size, color, label);
            field.multiline = field.wordWrap = lines != 1;
            var format:TextFormat = field.defaultTextFormat; format.leading = label ? 2 : 8;
            field.defaultTextFormat = format;
            var limit:Number = lines > 1 ? lines * (size * 1.2 + 8) : maxHeight;
            if (lines == 1 || limit) MenuStyle.fit(field, value, limit); else MenuStyle.setText(field, value);
            field.height = field.textHeight + 8; content.addChild(field);
            return field;
        }

        private function tag(value:String, left:Number, top:Number):Number
        {
            var label:TextField = text(value, left + 10, top + 3, 400, MenuStyle.SMALL_SIZE - 1, MenuStyle.MUTED, true, 1);
            label.width = label.textWidth + 8;
            graphics.lineStyle(1, MenuStyle.LINE); graphics.drawRect(left, top, label.width + 18, 28); graphics.lineStyle();
            return left + label.width + 20;
        }

        private function keycap(value:String, top:Number):TextField
        {
            var label:TextField = text(value, 0, top + 2, 260, MenuStyle.VALUE_SIZE - 1, MenuStyle.WHITE, true, 1);
            label.width = label.textWidth + 8; label.x = WIDTH - PAD - label.width - 8;
            graphics.lineStyle(1, 0x7D8F98); graphics.drawRect(label.x - 8, top, label.width + 16, 30); graphics.lineStyle();
            return label;
        }
    }
}
