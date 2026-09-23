package
{
    import flash.display.Sprite;
    import flash.text.TextField;
    import flash.text.TextFormat;

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

        public function LauncherCard(data:Object, position:int, width:Number, height:Number)
        {
            row = data; index = position; cardWidth = width; cardHeight = height;
            name = "launcher_" + position; mouseChildren = false; buttonMode = Boolean(row.editable);
            var large:Boolean = CONFIG::largeText;
            var compact:Boolean = height < 150;
            var diameter:Number = compact ? 48 : large ? 64 : 56;
            var top:Number = compact ? 12 : 18;
            tint = row.more ? Badge.MORE : Badge.color(row.mod + "/" + row.key);
            badge = new Badge(row.more ? String(row.badge) : Badge.initials(String(row.title)), diameter);
            badge.x = (width - diameter) / 2; badge.y = top; addChild(badge);
            var size:Number = compact ? (large ? 25 : 21) : (large ? 28 : 24);
            var titleTop:Number = top + diameter + (compact ? 6 : 12);
            var tagHeight:Number = row.editable ? 0 : size;
            title = field(String(row.title).toUpperCase(), titleTop, height - titleTop - tagHeight - 8, size);
            if (!row.editable) tag = field(tr("home.unavailable"), height - tagHeight - 8, tagHeight + 4, size - 5);
            select(false);
        }
        private function field(value:String, y:Number, height:Number, size:Number):TextField
        {
            var text:TextField = MenuStyle.field(value, 14, y, cardWidth - 28, height, size, MenuStyle.WHITE, true);
            var format:TextFormat = text.defaultTextFormat; format.align = "center";
            text.defaultTextFormat = format; text.setTextFormat(format);
            text.wordWrap = text.multiline = true;
            // Truncate at a fixed readable size; the detail card shows the full title.
            while (value.length && text.textHeight > height - 4) {
                value = value.substr(0, value.length - 1);
                MenuStyle.setText(text, value + "...");
            }
            addChild(text); return text;
        }
        public function select(selected:Boolean):void
        {
            graphics.clear();
            graphics.lineStyle(1, selected ? MenuStyle.WHITE : 0x34424B);
            graphics.beginFill(selected ? MenuStyle.WHITE : 0x0E1B23);
            graphics.drawRect(0, 0, cardWidth, cardHeight); graphics.endFill();
            badge.paint(selected ? MenuStyle.INK : row.editable ? tint : MenuStyle.LINE);
            title.textColor = selected ? MenuStyle.INK : row.editable ? MenuStyle.WHITE : MenuStyle.MUTED;
            if (tag) tag.textColor = selected ? 0x3D4F58 : MenuStyle.MUTED;
        }
    }
}
