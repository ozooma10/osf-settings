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
            if (row.more) {
                title = field(String(row.badge) + " " + String(row.title), 16, 0, height - 16, MenuStyle.BODY_SIZE - 1);
                center(title);
                title.height = title.textHeight + 4;
                title.y = (height - title.height) / 2;
                select(false); return;
            }
            var diameter:Number = large ? 48 : 40;
            tint = Badge.color(row.mod + "/" + row.key);
            badge = new Badge(Badge.initials(String(row.title)), diameter);
            badge.x = 16; badge.y = (height - diameter) / 2; addChild(badge);
            var size:Number = MenuStyle.BODY_SIZE - 1;
            var left:Number = badge.x + diameter + 14;
            var tagHeight:Number = row.editable ? 0 : size + 2;
            title = field(String(row.title), left, 0, height - 16 - tagHeight, size);
            title.height = title.textHeight + 4;
            title.y = (height - title.height - tagHeight) / 2;
            if (!row.editable) tag = field(tr("home.unavailable"), left, title.y + title.height + 4, tagHeight, size - 5);
            select(false);
        }
        private function center(text:TextField):void
        {
            var format:TextFormat = text.defaultTextFormat;
            format.align = "center";
            text.defaultTextFormat = format; text.setTextFormat(format);
        }
        private function field(value:String, left:Number, y:Number, height:Number, size:Number):TextField
        {
            var text:TextField = MenuStyle.field(value, left, y, cardWidth - left - 20, height, size);
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
            if (badge) badge.paint(selected ? MenuStyle.INK : row.editable ? tint : MenuStyle.LINE);
            title.textColor = selected ? MenuStyle.INK : row.editable ? MenuStyle.WHITE : MenuStyle.MUTED;
            if (tag) tag.textColor = selected ? 0x3D4F58 : MenuStyle.MUTED;
        }
    }
}
