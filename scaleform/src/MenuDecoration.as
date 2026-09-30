package
{
    import flash.display.Graphics;

    // Static Constellation instrument markings. Callers reserve the content area;
    // the ornament uses only the space left below it and never handles input.
    public final class MenuDecoration
    {
        private static const GOLD:uint = 0xD6B77D;
        private static const BLUE:uint = 0x719EB5;
        private static const RED:uint = 0xC65C45;

        public static function rule(g:Graphics, left:Number, right:Number, y:Number):void
        {
            g.lineStyle(1, MenuStyle.LINE); g.moveTo(left, y); g.lineTo(right, y);
            g.lineStyle(1, GOLD, 0.45);
            for (var x:Number = left + 32, i:int = 0; x < right - 24; x += 32, ++i) {
                g.moveTo(x, y); g.lineTo(x, y - (i % 4 == 0 ? 9 : 3));
            }
            var center:Number = left + MenuStyle.LIST_WIDTH / 2;
            g.drawCircle(center, y, 7);
            g.moveTo(center, y - 15); g.lineTo(center, y + 7);
            g.lineStyle(); g.beginFill(GOLD, 0.85);
            g.drawCircle(left, y, 2.5); g.drawCircle(right, y, 2.5); g.endFill();
        }

        public static function stripe(g:Graphics, left:Number, y:Number, width:Number):void
        {
            g.lineStyle(1, MenuStyle.LINE); g.moveTo(left, y); g.lineTo(left + width, y);
            g.lineStyle();
            var colors:Array = [BLUE, MenuStyle.WHITE, RED, GOLD];
            for (var i:int = 0; i < colors.length; ++i) {
                g.beginFill(colors[i], 0.9); g.drawRect(left + i * 48, y - 2, 46, 5); g.endFill();
            }
        }

        public static function corners(g:Graphics, left:Number, top:Number, width:Number, height:Number, color:uint = GOLD):void
        {
            g.lineStyle(1, MenuStyle.LINE, 0.35); g.drawRect(left, top, width, height);
            g.lineStyle(2, color, 0.75);
            for (var i:int = 0; i < 4; ++i) {
                var x:Number = left + (i % 2 ? width : 0), y:Number = top + (i < 2 ? 0 : height);
                var dx:Number = i % 2 ? -1 : 1, dy:Number = i < 2 ? 1 : -1;
                g.moveTo(x + dx * 22, y); g.lineTo(x, y); g.lineTo(x, y + dy * 22);
            }
            g.lineStyle();
        }

        public static function orbit(g:Graphics, left:Number, top:Number, width:Number, bottom:Number):void
        {
            var radius:Number = Math.min(width * 0.39, (bottom - top) / 2 - 26);
            if (radius < 100) return;
            var x:Number = left + width / 2, y:Number = (top + bottom) / 2;
            g.lineStyle(1, GOLD, 0.32);
            g.drawCircle(x, y, radius); g.drawCircle(x, y, radius * 0.73); g.drawCircle(x, y, radius * 0.45);
            g.lineStyle(1, MenuStyle.MUTED, 0.2);
            g.moveTo(x - radius - 20, y); g.lineTo(x + radius + 20, y);
            g.moveTo(x, y - radius - 20); g.lineTo(x, y + radius + 20);
            g.lineStyle(); g.beginFill(GOLD, 0.3);
            for (var i:int = 0; i < 72; ++i) {
                var angle:Number = i * Math.PI / 36;
                g.drawCircle(x + Math.cos(angle) * (radius + 12), y + Math.sin(angle) * (radius + 12), 0.8);
            }
            g.endFill();
            var bodies:Array = [{a:180, r:0.73, c:BLUE}, {a:8, r:0.73, c:RED},
                {a:90, r:1, c:GOLD}, {a:270, r:0.73, c:MenuStyle.MUTED}];
            for each (var body:Object in bodies) {
                angle = body.a * Math.PI / 180;
                g.beginFill(body.c, 0.55);
                g.drawCircle(x + Math.cos(angle) * radius * body.r, y + Math.sin(angle) * radius * body.r, 5);
                g.endFill();
            }
            g.lineStyle(1, GOLD, 0.65);
            g.moveTo(x, y - 20); g.lineTo(x + 4, y - 4); g.lineTo(x + 20, y);
            g.lineTo(x + 4, y + 4); g.lineTo(x, y + 20); g.lineTo(x - 4, y + 4);
            g.lineTo(x - 20, y); g.lineTo(x - 4, y - 4); g.lineTo(x, y - 20);
            g.lineStyle();
        }
    }
}
