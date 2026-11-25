package
{
    import flash.display.Graphics;

    // Colors arrive from native as uppercase "#RRGGBBAA"; setColor accepts the same text.
    // Channels are 0-255 integers; hue is 0-360 degrees, saturation and brightness 0-1.
    public final class ColorSetting
    {
        private static const DIGITS:String = "0123456789ABCDEF";

        // {r, g, b, a} for "#RRGGBB" or "#RRGGBBAA", or null for anything else.
        public static function parse(text:String):Object
        {
            if (!text || (text.length != 7 && text.length != 9) || text.charAt(0) != "#") return null;
            var channels:Array = [];
            for (var i:int = 1; i < text.length; i += 2) {
                var high:int = digit(text.charCodeAt(i)), low:int = digit(text.charCodeAt(i + 1));
                if (high < 0 || low < 0) return null;
                channels.push(high * 16 + low);
            }
            return {r:channels[0], g:channels[1], b:channels[2], a:channels.length > 3 ? channels[3] : 255};
        }
        private static function digit(code:Number):int
        {
            if (code >= 48 && code <= 57) return int(code - 48);
            if (code >= 65 && code <= 70) return int(code - 55);
            if (code >= 97 && code <= 102) return int(code - 87);
            return -1;
        }
        private static function hex(channel:int):String
        {
            channel = Math.max(0, Math.min(255, channel));
            return DIGITS.charAt(channel >> 4) + DIGITS.charAt(channel & 15);
        }
        // The eight-digit form native stores and setColor receives.
        public static function format(color:Object):String
        {
            return "#" + hex(color.r) + hex(color.g) + hex(color.b) + hex(color.a);
        }
        // What players read: six digits unless the setting edits opacity.
        public static function display(color:Object, alpha:Boolean):String
        {
            return "#" + hex(color.r) + hex(color.g) + hex(color.b) + (alpha ? hex(color.a) : "");
        }
        public static function text(row:Object, value:*):String
        {
            var color:Object = parse(String(value));
            return color ? display(color, Boolean(row.alpha)) : String(value);
        }
        public static function rgb(color:Object):uint
        {
            return (uint(color.r) << 16) | (uint(color.g) << 8) | uint(color.b);
        }

        public static function toHsv(color:Object):Object
        {
            var r:Number = color.r / 255, g:Number = color.g / 255, b:Number = color.b / 255;
            var max:Number = Math.max(r, g, b), min:Number = Math.min(r, g, b), range:Number = max - min;
            var hue:Number = 0;
            if (range > 0) {
                if (max == r) hue = 60 * (((g - b) / range) % 6);
                else if (max == g) hue = 60 * ((b - r) / range + 2);
                else hue = 60 * ((r - g) / range + 4);
                if (hue < 0) hue += 360;
            }
            return {h:hue, s:max > 0 ? range / max : 0, v:max};
        }
        // Rounded channels for hue 0-360 and saturation/brightness 0-1.
        public static function fromHsv(hue:Number, saturation:Number, brightness:Number, alpha:int = 255):Object
        {
            hue = ((hue % 360) + 360) % 360;
            var chroma:Number = brightness * saturation;
            var x:Number = chroma * (1 - Math.abs((hue / 60) % 2 - 1));
            var m:Number = brightness - chroma;
            var r:Number = 0, g:Number = 0, b:Number = 0;
            if (hue < 60) { r = chroma; g = x; }
            else if (hue < 120) { r = x; g = chroma; }
            else if (hue < 180) { g = chroma; b = x; }
            else if (hue < 240) { g = x; b = chroma; }
            else if (hue < 300) { r = x; b = chroma; }
            else { r = chroma; b = x; }
            return {r:Math.round((r + m) * 255), g:Math.round((g + m) * 255), b:Math.round((b + m) * 255), a:alpha};
        }

        // A filled swatch; translucent colors sit on a checkerboard so their opacity reads.
        public static function drawSwatch(target:Graphics, x:Number, y:Number, width:Number, height:Number, color:Object, border:uint):void
        {
            if (color.a < 255) checkerboard(target, x, y, width, height, Math.max(4, Math.round(height / 4)));
            target.lineStyle();
            target.beginFill(rgb(color), color.a / 255); target.drawRect(x, y, width, height); target.endFill();
            target.lineStyle(1, border); target.drawRect(x, y, width, height); target.lineStyle();
        }
        public static function checkerboard(target:Graphics, x:Number, y:Number, width:Number, height:Number, cell:Number):void
        {
            target.lineStyle();
            target.beginFill(0xC8CDCF); target.drawRect(x, y, width, height); target.endFill();
            target.beginFill(0x7D878C);
            for (var row:int = 0; row * cell < height; ++row) {
                for (var column:int = row % 2; column * cell < width; column += 2) {
                    target.drawRect(x + column * cell, y + row * cell, Math.min(cell, width - column * cell), Math.min(cell, height - row * cell));
                }
            }
            target.endFill();
        }
    }
}
