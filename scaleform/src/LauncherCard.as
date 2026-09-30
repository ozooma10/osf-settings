package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    // Cards show only what identifies an interface; Home's detail card describes the selection.
    public final class LauncherCard extends Sprite
    {
        public var row:Object;
        public var index:int;
        private var cardWidth:Number;
        private var cardHeight:Number;
        private var badge:Badge;
        private var tint:uint;
        private var title:TextField;
        private var tag:TextField;
        private var isSelected:Boolean;
        private var isHovered:Boolean;
        private var painted:Boolean;
        private var paintedHover:Boolean;
        private var wasLoading:Boolean;

        public function LauncherCard(data:Object, position:int, width:Number, height:Number)
        {
            row = data; index = position; cardWidth = width; cardHeight = height;
            wasLoading = Boolean(row.loading);
            addEventListener(MouseEvent.ROLL_OVER, hover); addEventListener(MouseEvent.ROLL_OUT, hover);
            name = "launcher_" + position; mouseChildren = false; buttonMode = Boolean(row.editable);
            var large:Boolean = CONFIG::largeText;
            var diameter:Number = large ? 82 : 70;
            tint = row.more ? MenuStyle.ACCENT : Badge.color(row.mod + "/" + row.key);
            badge = new Badge(row.more ? "" : Badge.initials(String(row.title)), diameter);
            badge.x = 20; badge.y = (height - diameter) / 2; addChild(badge);
            var size:Number = MenuStyle.BODY_SIZE;
            var left:Number = badge.x + diameter + 18;
            // Status takes the caption's place without changing the card's layout.
            var caption:String = row.more ? "" : !row.editable ? tr("home.unavailable") :
                row.loading ? tr("home.loading") : String(row.modTitle || "");
            var tagHeight:Number = caption ? MenuStyle.SMALL_SIZE + 13 : 0;
            title = field(String(row.title), left, 0, Math.min(size * 2 + 12, height - 28 - tagHeight), size);
            title.height = title.textHeight + 4;
            title.y = (height - title.height - (caption ? tagHeight + 4 : 0)) / 2;
            if (caption) tag = field(caption, left, title.y + title.height + 4, tagHeight, MenuStyle.SMALL_SIZE + 1);
            select(false);
        }
        // Keep clips and listeners when only the backing snapshot changed.
        public function reuse(data:Object, position:int, width:Number, height:Number):Boolean
        {
            if (cardWidth != width || cardHeight != height || row.mod != data.mod || row.key != data.key ||
                row.title != data.title || row.modTitle != data.modTitle || row.editable != data.editable ||
                row.more != data.more || wasLoading != Boolean(data.loading)) return false;
            row = data; index = position; name = "launcher_" + position;
            return true;
        }
        private function field(value:String, left:Number, y:Number, height:Number, size:Number):TextField
        {
            var text:TextField = MenuStyle.field(value, left, y, cardWidth - left - 46, height, size);
            text.wordWrap = text.multiline = true;
            // Truncate at a fixed readable size; the detail card shows the full title.
            MenuStyle.fit(text, value, height - 4);
            addChild(text); return text;
        }
        private function hover(event:MouseEvent):void { isHovered = event.type == MouseEvent.ROLL_OVER; select(isSelected); }
        public function select(selected:Boolean):void
        {
            if (painted && isSelected == selected && paintedHover == isHovered) return;
            painted = true; paintedHover = isHovered;
            isSelected = selected;
            graphics.clear();
            graphics.lineStyle(1, selected ? MenuStyle.WHITE : isHovered ? MenuStyle.LINE : 0x34424B);
            graphics.beginFill(selected ? MenuStyle.WHITE : isHovered ? 0x182A33 : 0x0E1B23);
            graphics.drawRect(0, 0, cardWidth, cardHeight); graphics.endFill();
            var accent:uint = selected ? MenuStyle.INK : row.editable ? tint : MenuStyle.LINE;
            graphics.lineStyle(2, accent, 0.65);
            graphics.moveTo(24, 1); graphics.lineTo(1, 1); graphics.lineTo(1, 24);
            graphics.lineStyle(2, selected ? MenuStyle.INK : row.editable ? MenuStyle.WHITE : MenuStyle.LINE);
            graphics.moveTo(cardWidth - 30, cardHeight / 2 - 8);
            graphics.lineTo(cardWidth - 22, cardHeight / 2); graphics.lineTo(cardWidth - 30, cardHeight / 2 + 8);
            graphics.lineStyle();
            badge.paint(accent);
            if (row.more) {
                var diameter:Number = CONFIG::largeText ? 82 : 70;
                graphics.beginFill(accent);
                for (var i:int = 0; i < 4; ++i)
                    graphics.drawRect(badge.x + diameter / 2 - 10 + i % 2 * 12, cardHeight / 2 - 10 + int(i / 2) * 12, 8, 8);
                graphics.endFill();
            }
            title.textColor = selected ? MenuStyle.INK : row.editable ? MenuStyle.WHITE : MenuStyle.MUTED;
            if (tag) tag.textColor = selected ? 0x3D4F58 : row.loading && row.editable ? MenuStyle.ACCENT : MenuStyle.MUTED;
        }
    }
}
