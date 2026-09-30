package
{
    import flash.display.Graphics;

    // Issues need more reading room than the setting-value detail column.
    public final class IssueStyle
    {
        public static const LIST_WIDTH:Number = 790;
        public static const DETAIL_X:Number = MenuStyle.LEFT + LIST_WIDTH + 32;
        public static const TOP:Number = CONFIG::largeText ? 180 : 166;
        public static const ROW_HEIGHT:Number = CONFIG::largeText ? 132 : 110;
        public static const ERROR:uint = 0xF07865;
        public static const WARNING:uint = 0xE7B566;

        public static function color(severity:String):uint { return severity == "ERROR" ? ERROR : WARNING; }

        public static function icon(g:Graphics, x:Number, y:Number, error:Boolean, radius:Number = 18):void
        {
            g.lineStyle(2, error ? ERROR : WARNING);
            if (error) g.drawCircle(x, y, radius);
            else {
                g.moveTo(x, y - radius); g.lineTo(x + radius, y + radius * 0.85);
                g.lineTo(x - radius, y + radius * 0.85); g.lineTo(x, y - radius);
            }
            g.moveTo(x, y - radius * 0.45); g.lineTo(x, y + radius * 0.2);
            g.moveTo(x, y + radius * 0.5); g.lineTo(x, y + radius * 0.55);
            g.lineStyle();
        }
    }
}
