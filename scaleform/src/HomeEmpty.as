package
{
    import flash.display.Shape;
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // Home when no installed mod uses OSF Settings: say so, and point to the one
    // thing the player can still do here, which is change the key that opens it.
    public final class HomeEmpty extends Sprite
    {
        private static const TOP:Number = MenuStyle.TABS_LINE;
        private static const BOTTOM:Number = MenuStyle.FOOTER_LINE;
        private static const CENTER:Number = (MenuStyle.LEFT + MenuStyle.RIGHT) / 2;
        private var stack:Sprite = new Sprite();
        private var keyLine:Sprite = new Sprite();
        private var keybindings:Function;
        private var shownKey:String = null;

        public function HomeEmpty(openKeybindings:Function)
        {
            keybindings = openKeybindings; name = "homeEmpty"; visible = false;
            addChild(stack);
            var art:Sprite = orbit(); stack.addChild(art);
            var large:Boolean = CONFIG::largeText;
            var title:TextField = centered(tr("home.emptyTitle"), 1200, large ? 50 : 44, MenuStyle.WHITE, false);
            title.y = art.getBounds(art).bottom + 28; stack.addChild(title);
            var body:TextField = centered(tr("home.emptyBody"), 940, large ? 30 : 27, MenuStyle.MUTED, false);
            body.y = title.y + title.height + 14; stack.addChild(body);
            keyLine.y = body.y + body.height + 40; stack.addChild(keyLine);
            show("");
        }

        public function show(key:String):void
        {
            if (key == shownKey) return;
            shownKey = key;
            while (keyLine.numChildren) keyLine.removeChildAt(0);
            keyLine.graphics.clear();
            var size:Number = CONFIG::largeText ? 25 : 22;
            var x:Number = 0;
            if (key) {
                var cap:TextField = MenuStyle.field(key, 12, 4, 200, 36, size, MenuStyle.WHITE, true);
                cap.width = cap.textWidth + 8; keyLine.addChild(cap);
                keyLine.graphics.lineStyle(1, 0x7D8F98); keyLine.graphics.drawRect(0, 0, cap.width + 24, 42);
                x = cap.width + 42;
                var opens:TextField = MenuStyle.field(tr("home.opensMenu"), x, 7, 500, 36, size, MenuStyle.MUTED, true);
                opens.width = opens.textWidth + 8; keyLine.addChild(opens);
                x += opens.width + 26;
                keyLine.graphics.lineStyle(1, 0x2F424B); keyLine.graphics.moveTo(x, 6); keyLine.graphics.lineTo(x, 36);
                x += 26;
            }
            var link:Sprite = new Sprite(); link.name = "homeKeybindings"; link.buttonMode = true; link.mouseChildren = false;
            var label:TextField = MenuStyle.field(tr("home.changeInKeybindings") + "  >", 0, 7, 600, 36, size, MenuStyle.WHITE, true);
            label.width = label.textWidth + 8; link.addChild(label);
            link.graphics.beginFill(0, 0); link.graphics.drawRect(0, 0, label.width, 42); link.graphics.endFill();
            link.x = x; link.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { keybindings(); });
            keyLine.addChild(link);
            keyLine.x = -keyLine.width / 2;
            // Center the whole block in the content area between the dividers.
            stack.x = CENTER; stack.y = TOP + Math.max(0, (BOTTOM - TOP - stack.height) / 2);
        }

        private function centered(value:String, width:Number, size:Number, color:uint, label:Boolean):TextField
        {
            var field:TextField = MenuStyle.field("", -width / 2, 0, width, 40, size, color, label);
            field.multiline = field.wordWrap = true;
            var format:TextFormat = field.defaultTextFormat; format.align = "center"; format.leading = 8;
            field.defaultTextFormat = format; MenuStyle.setText(field, value);
            field.height = field.textHeight + 8;
            return field;
        }

        // A planet with an empty orbit and one small moon, drawn to match the menu's line art.
        private function orbit():Sprite
        {
            var art:Sprite = new Sprite();
            var ring:Shape = new Shape(); ring.rotation = -18;
            ring.graphics.lineStyle(2, MenuStyle.LINE); ring.graphics.drawEllipse(-84, -30, 168, 60);
            var moon:Number = -40 * Math.PI / 180;
            ring.graphics.lineStyle(); ring.graphics.beginFill(MenuStyle.ACCENT);
            ring.graphics.drawCircle(84 * Math.cos(moon), 30 * Math.sin(moon), 7); ring.graphics.endFill();
            var planet:Shape = new Shape();
            planet.graphics.lineStyle(2, 0x2F424B);
            for (var i:int = 0; i < 12; ++i) {
                var from:Number = i * Math.PI / 6, to:Number = from + Math.PI / 12;
                planet.graphics.moveTo(24 * Math.cos(from), 24 * Math.sin(from));
                for (var step:int = 1; step <= 4; ++step) {
                    var angle:Number = from + (to - from) * step / 4;
                    planet.graphics.lineTo(24 * Math.cos(angle), 24 * Math.sin(angle));
                }
            }
            planet.graphics.lineStyle(2, MenuStyle.WHITE); planet.graphics.drawCircle(0, 0, 38);
            art.addChild(ring); art.addChild(planet);
            ring.y = planet.y = 60;
            return art;
        }
    }
}
