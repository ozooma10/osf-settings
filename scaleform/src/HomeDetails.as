package
{
    import flash.display.Sprite;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // Home's right-hand card. It describes whatever is selected, a mod or an interface,
    // from a plain detail object the menu builds. Opening stays on the footer's Accept.
    public final class HomeDetails extends Sprite
    {
        private static const WIDTH:Number = 634;
        private static const BOTTOM:Number = 894;
        private static const PAD:Number = 28;
        private var content:Sprite = new Sprite();
        private var signature:String = "";
        private var selected:Object;

        public function HomeDetails()
        {
            name = "homeDetails"; x = 1210; mouseEnabled = false; addChild(content);
        }

        public function get detail():Object { return selected; }

        // detail: {title, badge, tint, subtitle, description, warning, chips:[], hotkeys:[{title, key}]}
        public function show(detail:Object, top:Number):void
        {
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
            var large:Boolean = CONFIG::largeText;
            var badge:Badge = new Badge(detail.badge, 72); badge.paint(detail.tint);
            badge.x = PAD; badge.y = PAD; content.addChild(badge);
            var title:TextField = text(detail.title, PAD + 92, PAD, WIDTH - PAD * 2 - 92, large ? 38 : 34, MenuStyle.WHITE, false, 2);
            title.y = PAD + Math.max(0, (72 - title.height) / 2);
            var cursor:Number = Math.max(PAD + 72, title.y + title.height) + 20;
            if (detail.subtitle) cursor = text(String(detail.subtitle).toUpperCase(), PAD, cursor, WIDTH - PAD * 2, large ? 21 : 19, MenuStyle.MUTED, true, 1).y + 36;

            // Content flows top-down. Hotkeys get the room left after the chips and, when there
            // is one, a line of description; the description takes the rest.
            var limit:Number = height - PAD;
            var chipBlock:Number = detail.chips.length ? 54 : 0;
            var hotkeys:Array = detail.hotkeys || [];
            var rowHeight:Number = large ? 46 : 40;
            var reserve:Number = detail.warning || detail.description ? 60 : 0;
            var room:int = hotkeys.length ? Math.max(0, Math.floor((limit - cursor - reserve - chipBlock - 21) / rowHeight)) : 0;
            var shown:int = hotkeys.length > room ? Math.max(0, room - 1) : hotkeys.length;
            var more:Boolean = shown < hotkeys.length && room > 0;
            var hotkeyBlock:Number = shown || more ? 21 + (shown + (more ? 1 : 0)) * rowHeight : 0;
            var summary:String = detail.warning || detail.description;
            var space:Number = limit - cursor - chipBlock - hotkeyBlock;
            if (summary && space > 30) {
                var body:TextField = text(summary, PAD, cursor, WIDTH - PAD * 2, large ? 28 : 25,
                    detail.warning ? MenuStyle.ACCENT : MenuStyle.MUTED, false, 0, space - 16);
                cursor = body.y + body.height + 12;
            }
            var chipX:Number = PAD;
            for each (var chip:String in detail.chips) chipX = tag(chip, chipX, cursor) + 10;
            cursor += chipBlock;
            if (shown || more) {
                graphics.lineStyle(1, 0x34424B); graphics.moveTo(PAD, cursor); graphics.lineTo(WIDTH - PAD, cursor); graphics.lineStyle();
                var rowTop:Number = cursor + 20;
                for (var i:int = 0; i < shown; ++i, rowTop += rowHeight) {
                    var key:TextField = keycap(String(hotkeys[i].key), rowTop);
                    text(hotkeys[i].title, PAD, rowTop + 4, key.x - PAD - 30, large ? 26 : 24, MenuStyle.WHITE, false, 1);
                }
                if (more) text(tr("home.moreHotkeys", {count:hotkeys.length - shown}), PAD, rowTop + 6, WIDTH - PAD * 2, large ? 21 : 19, MenuStyle.MUTED, true, 1);
            }
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
            if (lines == 1) MenuStyle.fit(field, value); else MenuStyle.setText(field, value);
            var limit:Number = lines > 1 ? lines * (size * 1.2 + 8) : maxHeight;
            while (limit && value.length && field.textHeight > limit) {
                value = value.substr(0, value.length - 1);
                MenuStyle.setText(field, value + "...");
            }
            field.height = field.textHeight + 8; content.addChild(field);
            return field;
        }

        private function tag(value:String, left:Number, top:Number):Number
        {
            var label:TextField = text(value, left + 12, top + 4, 400, CONFIG::largeText ? 20 : 18, MenuStyle.MUTED, true, 1);
            label.width = label.textWidth + 8;
            graphics.lineStyle(1, MenuStyle.LINE); graphics.drawRect(left, top, label.width + 20, 34); graphics.lineStyle();
            return left + label.width + 20;
        }

        private function keycap(value:String, top:Number):TextField
        {
            var label:TextField = text(value, 0, top + 3, 260, CONFIG::largeText ? 24 : 22, MenuStyle.WHITE, true, 1);
            label.width = label.textWidth + 8; label.x = WIDTH - PAD - label.width - 10;
            graphics.lineStyle(1, 0x7D8F98); graphics.drawRect(label.x - 10, top, label.width + 20, 36); graphics.lineStyle();
            return label;
        }
    }
}
