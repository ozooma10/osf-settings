package
{
    import flash.display.DisplayObject;
    import flash.display.Graphics;
    import flash.display.MovieClip;
    import flash.geom.Rectangle;
    import flash.text.TextField;
    import flash.text.TextFormat;

    public final class MenuStyle
    {
        public static const INK:uint = 0x08151C;
        public static const WHITE:uint = 0xF1F2EC;
        public static const MUTED:uint = 0xAABCC6;
        public static const ROW:uint = 0x24363F;
        // Under the pointer but not selected: a step lighter than a row.
        public static const HOVER:uint = 0x344B56;
        public static const LINE:uint = 0x52646D;
        public static const ACCENT:uint = 0xDE9D65;
        // Desk-distance layout on the 1920x1080 stage: navigation at the left, then the
        // list and the detail column. The _LRG movie keeps roomier sizes for Starfield's
        // large menu text option.
        public static const NAV_WIDTH:Number = 420;
        public static const LEFT:Number = 452;
        public static const RIGHT:Number = 1888;
        public static const TITLE_TOP:Number = 26;
        public static const TITLE_SIZE:Number = CONFIG::largeText ? 40 : 34;
        public static const HEADER_LINE:Number = CONFIG::largeText ? 82 : 76;
        // Home and Issues label the list above it; mod pages start the list here.
        public static const SECTION_TOP:Number = 102;
        public static const SECTION_SIZE:Number = CONFIG::largeText ? 24 : 20;
        public static const LIST_TOP:Number = CONFIG::largeText ? 148 : 142;
        public static const LIST_BOTTOM:Number = CONFIG::largeText ? 940 : 990;
        public static const LIST_WIDTH:Number = 940;
        // Every setting uses this control column, including native binding cells.
        public static const CONTROL_WIDTH:Number = 450;
        public static const CONTROL_X:Number = LIST_WIDTH - 26 - CONTROL_WIDTH;
        public static const VALUE_INSET:Number = 40;
        public static const VALUE_WIDTH:Number = CONTROL_WIDTH - 2 * VALUE_INSET;
        public static const SLIDER_WIDTH:Number = 330;
        public static const SLIDER_VALUE_WIDTH:Number = 100;
        public static const BINDING_GAP:Number = 24;
        public static const BINDING_WIDTH:Number = (CONTROL_WIDTH - BINDING_GAP) / 2;
        public static const LIST_HEIGHT:Number = LIST_BOTTOM - LIST_TOP;
        public static const ROW_HEIGHT:Number = CONFIG::largeText ? 56 : 44;
        // Keybinding and issue rows carry a second line under the title.
        public static const TALL_ROW_HEIGHT:Number = CONFIG::largeText ? 76 : 62;
        public static const ROW_GAP:Number = 2;
        public static const BODY_SIZE:Number = CONFIG::largeText ? 26 : 21;
        public static const VALUE_SIZE:Number = CONFIG::largeText ? 22 : 18;
        public static const SMALL_SIZE:Number = CONFIG::largeText ? 20 : 16;
        public static const DETAIL_X:Number = 1424;
        public static const DETAIL_WIDTH:Number = RIGHT - DETAIL_X;
        public static const DETAIL_TITLE_SIZE:Number = CONFIG::largeText ? 32 : 27;
        public static const DETAIL_BODY_SIZE:Number = CONFIG::largeText ? 24 : 20;
        public static const FOOTER_LINE:Number = CONFIG::largeText ? 954 : 1004;
        public static const FOOTER_Y:Number = 1042;

        public static function centerControl(control:DisplayObject, x:Number, width:Number, rowHeight:Number):void
        {
            // Native clips need not have their registration point at the top left.
            var bounds:Rectangle = control.getBounds(control.parent);
            control.x += x + (width - bounds.width) / 2 - bounds.x;
            control.y += (rowHeight - bounds.height) / 2 - bounds.y;
        }
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
        // Fit a single line by width, or wrapped text by the supplied height.
        public static function fit(field:TextField, text:String, maxHeight:Number = 0):void
        {
            setText(field, text);
            if (!text.length || (maxHeight ? field.textHeight <= maxHeight : field.textWidth <= field.width - 6)) return;
            var low:int = 0, high:int = text.length - 1;
            var result:String = "...";
            while (low <= high) {
                var middle:int = (low + high) >>> 1;
                var end:int = middle;
                if (end && text.charCodeAt(end - 1) >= 0xD800 && text.charCodeAt(end - 1) <= 0xDBFF) --end;
                var candidate:String = text.substr(0, end) + "...";
                setText(field, candidate);
                if (maxHeight ? field.textHeight <= maxHeight : field.textWidth <= field.width - 6) {
                    result = candidate; low = middle + 1;
                } else high = middle - 1;
            }
            setText(field, result);
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
