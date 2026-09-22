package
{
    import flash.display.Graphics;
    import flash.display.MovieClip;
    import flash.text.TextField;
    import flash.text.TextFormat;

    public final class MenuStyle
    {
        public static const INK:uint = 0x08151C;
        public static const WHITE:uint = 0xF1F2EC;
        public static const MUTED:uint = 0xAABCC6;
        public static const ROW:uint = 0x24363F;
        public static const LINE:uint = 0x52646D;
        public static const ACCENT:uint = 0xDE9D65;
        public static const LEFT:Number = 116;
        public static const RIGHT:Number = 1844;
        public static const LIST_WIDTH:Number = 1016;
        public static const LIST_TOP:Number = 362;
        public static const LIST_HEIGHT:Number = 506;
        public static const ROW_HEIGHT:Number = CONFIG::largeText ? 96 : 78;
        public static const BODY_SIZE:Number = CONFIG::largeText ? 32 : 28;

        public static function field(text:String, x:Number, y:Number, width:Number, height:Number,
                                     size:Number, color:uint = WHITE, label:Boolean = false):TextField
        {
            // Keep the DefineEditText font binding authored by build-scaleform.
            var symbol:MovieClip = label ? new MenuLabelField() : new MenuBodyField();
            var result:TextField = symbol.getChildAt(0) as TextField;
            symbol.removeChild(result);
            var format:TextFormat = new TextFormat(); format.size = size; format.color = color;
            result.defaultTextFormat = format;
            result.textColor = color; setText(result, text);
            result.x = x; result.y = y; result.width = width; result.height = height;
            result.selectable = false; result.mouseEnabled = false; return result;
        }
        public static function setText(field:TextField, text:String):void
        {
            // Apply the requested format to authored text as well as future
            // text; relying on defaultTextFormat left in-game labels unformatted.
            var format:TextFormat = field.defaultTextFormat; format.color = field.textColor;
            field.text = text; field.defaultTextFormat = format; field.setTextFormat(format);
        }
        public static function fit(field:TextField, text:String):void
        {
            setText(field, text);
            if (field.textWidth <= field.width - 6) return;
            while (text.length && field.textWidth > field.width - 6) {
                text = text.substr(0, text.length - 1);
                if (text.length && text.charCodeAt(text.length - 1) >= 0xD800 && text.charCodeAt(text.length - 1) <= 0xDBFF)
                    text = text.substr(0, text.length - 1);
                setText(field, text + "...");
            }
        }
        public static function diamond(graphics:Graphics, x:Number, y:Number, color:uint):void
        {
            graphics.beginFill(color);
            graphics.moveTo(x, y - 7); graphics.lineTo(x + 7, y);
            graphics.lineTo(x, y + 7); graphics.lineTo(x - 7, y); graphics.lineTo(x, y - 7);
            graphics.endFill();
        }
    }
}
