package
{
    import flash.display.Sprite;
    import flash.text.TextField;
    import flash.text.TextFormat;

    public final class LauncherCard extends Sprite
    {
        public var row:Object;
        public var index:int;
        private var cardWidth:Number;
        private var cardHeight:Number;

        public function LauncherCard(data:Object, position:int, width:Number, height:Number)
        {
            row = data; index = position; cardWidth = width; cardHeight = height;
            name = "launcher_" + position; mouseChildren = false; buttonMode = Boolean(row.editable);
            var large:Boolean = CONFIG::largeText;
            var badge:Sprite = makeBadge(large ? 108 : 96);
            badge.x = (width - badge.width) / 2; badge.y = large ? 35 : 42;
            if (!row.editable) badge.alpha = 0.5;
            addChild(badge);
            field(String(row.title).toUpperCase(), 169, 103, large ? 38 : 32, MenuStyle.WHITE, true);
            field(String(row.hint || ""), 289, 100, large ? 30 : 26, MenuStyle.MUTED);
            field(String(row.modTitle || row.mod).toUpperCase(), 397, 54, large ? 25 : 23, MenuStyle.MUTED, true);
            field(row.editable ? "OPEN  >" : "UNAVAILABLE", height - 46, 38,
                large ? 28 : 25, row.editable ? 0x9DC4D6 : MenuStyle.MUTED, true);
            select(false);
        }
        private function makeBadge(diameter:Number):Sprite
        {
            var badge:Sprite = new Sprite();
            var colors:Array = [0xB39BCF, 0x8DB8CD, 0xA1BD7D, 0xCE99B2, 0xD4AB72, 0xA3A8DC];
            var identity:String = row.mod + "/" + row.key; var hash:uint = 5381;
            for (var i:int = 0; i < identity.length; ++i) hash = uint(hash * 33 + identity.charCodeAt(i));
            var color:uint = colors[hash % colors.length]; var radius:Number = diameter / 2;
            badge.graphics.lineStyle(1, color, 0.8); badge.graphics.drawCircle(radius, radius, radius - 1);
            badge.graphics.lineStyle(1, color, 0.25); badge.graphics.drawCircle(radius, radius, radius - 5);
            badge.graphics.lineStyle(); badge.graphics.beginFill(color, 0.7);
            for each (var x:Number in [8, diameter - 8]) {
                badge.graphics.moveTo(x, radius - 2); badge.graphics.lineTo(x + 2, radius);
                badge.graphics.lineTo(x, radius + 2); badge.graphics.lineTo(x - 2, radius); badge.graphics.lineTo(x, radius - 2);
            }
            badge.graphics.endFill();
            var words:Array = String(row.title).replace(/^[\s\-_.]+|[\s\-_.]+$/g, "").split(/[\s\-_.]+/);
            var first:String = character(String(words[0] || "?"));
            var initials:String = first + character(words.length > 1 ? String(words[1]) : String(words[0]).substr(first.length));
            var text:TextField = MenuStyle.field(initials.toUpperCase(), 15, 0, diameter - 30, diameter, diameter * 0.32, color, true);
            var format:TextFormat = text.defaultTextFormat; format.align = "center";
            text.defaultTextFormat = format; text.setTextFormat(format);
            text.y = (diameter - text.textHeight) / 2 - 2; badge.addChild(text);
            return badge;
        }
        private static function character(text:String):String
        {
            var code:Number = text.charCodeAt(0);
            return text.substr(0, code >= 0xD800 && code <= 0xDBFF ? 2 : 1);
        }
        private function field(value:String, y:Number, height:Number, size:Number, color:uint, condensed:Boolean = false):void
        {
            var text:TextField = MenuStyle.field(value, 18, y, cardWidth - 36, height, size, color, condensed);
            var format:TextFormat = text.defaultTextFormat; format.align = "center";
            text.defaultTextFormat = format; text.setTextFormat(format);
            text.wordWrap = text.multiline = true;
            // Truncate at a fixed readable size. The footer also describes the selected card.
            while (value.length && text.textHeight > height - 4) {
                value = value.substr(0, value.length - 1);
                MenuStyle.setText(text, value + "...");
            }
            addChild(text);
        }
        public function select(selected:Boolean):void
        {
            graphics.clear();
            graphics.lineStyle(1, selected ? 0xB7C4CC : 0x34424B);
            graphics.beginFill(selected ? 0x1B252D : 0x0E1B23);
            graphics.drawRect(0, 0, cardWidth, cardHeight); graphics.endFill();
        }
    }
}
