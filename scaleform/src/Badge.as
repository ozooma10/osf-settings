package
{
    import flash.display.Sprite;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // The round initials badge shared by interface cards and Home's detail card.
    public final class Badge extends Sprite
    {
        public static const MORE:uint = 0x9DC4D6;
        private static const COLORS:Array = [0xB39BCF, 0x8DB8CD, 0xA1BD7D, 0xCE99B2, 0xD4AB72, 0xA3A8DC];
        private var diameter:Number;
        private var label:TextField;

        public function Badge(text:String, size:Number)
        {
            diameter = size; mouseEnabled = false; mouseChildren = false;
            label = MenuStyle.field(text, 9, 0, size - 18, size, size * 0.32, MORE, true);
            var format:TextFormat = label.defaultTextFormat; format.align = "center";
            label.defaultTextFormat = format; label.setTextFormat(format);
            label.y = (size - label.textHeight) / 2 - 2; addChild(label);
        }
        public static function color(identity:String):uint
        {
            var hash:uint = 5381;
            for (var i:int = 0; i < identity.length; ++i) hash = uint(hash * 33 + identity.charCodeAt(i));
            return COLORS[hash % COLORS.length];
        }
        public static function initials(title:String):String
        {
            var words:Array = title.replace(/^[\s\-_.]+|[\s\-_.]+$/g, "").split(/[\s\-_.]+/);
            var first:String = character(String(words[0] || "?"));
            return (first + character(words.length > 1 ? String(words[1]) : String(words[0]).substr(first.length))).toUpperCase();
        }
        private static function character(text:String):String
        {
            var code:Number = text.charCodeAt(0);
            return text.substr(0, code >= 0xD800 && code <= 0xDBFF ? 2 : 1);
        }
        public function paint(tint:uint):void
        {
            var radius:Number = diameter / 2;
            graphics.clear();
            graphics.lineStyle(1, tint, 0.8); graphics.drawCircle(radius, radius, radius - 1);
            graphics.lineStyle(1, tint, 0.25); graphics.drawCircle(radius, radius, radius - 5);
            graphics.lineStyle(); graphics.beginFill(tint, 0.7);
            for each (var x:Number in [8, diameter - 8]) {
                graphics.moveTo(x, radius - 2); graphics.lineTo(x + 2, radius);
                graphics.lineTo(x, radius + 2); graphics.lineTo(x - 2, radius); graphics.lineTo(x, radius - 2);
            }
            graphics.endFill();
            label.textColor = tint;
        }
    }
}
