package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;
    import flash.text.TextField;
    import flash.text.TextFormat;

    public final class IssueDetails extends Sprite
    {
        private static const WIDTH:Number = 634;
        private static const HEIGHT:Number = 506;
        private var viewport:Sprite = new Sprite();
        private var content:Sprite = new Sprite();
        private var position:Number = 0;
        private var extent:Number = 0;
        private var selected:Object;

        public function IssueDetails()
        {
            x = 1210; y = 363;
            viewport.scrollRect = new Rectangle(0, 0, WIDTH, HEIGHT);
            viewport.addChild(content); addChild(viewport);
            // A transparent hit area lets the wheel work between paragraphs too.
            graphics.beginFill(0, 0); graphics.drawRect(0, 0, WIDTH, HEIGHT); graphics.endFill();
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
        }

        public function get scrollable():Boolean { return extent > HEIGHT; }
        CONFIG::testHarness {
            public function testState():Object { return {position:position, extent:extent, height:HEIGHT}; }
        }

        public function show(row:Object):void
        {
            if (selected == row) return;
            var preserve:Boolean = selected && row && selected.mod == row.mod && selected.id == row.id;
            selected = row;
            while (content.numChildren) content.removeChildAt(0);
            content.graphics.clear(); extent = 0;
            if (row) {
                line("SELECTED ISSUE", 21, MenuStyle.MUTED, true, 16);
                line(row.modTitle + " / " + row.severity, CONFIG::largeText ? 25 : 23, MenuStyle.MUTED, true, 12);
                line(row.title, CONFIG::largeText ? 38 : 34, MenuStyle.WHITE, false, 32);
                if (row.impact) {
                    line("WHAT THIS AFFECTS", 21, MenuStyle.MUTED, true, 12);
                    line(row.impact, CONFIG::largeText ? 30 : 27, MenuStyle.MUTED, false, 30);
                }
                if (row.nextSteps) {
                    if (row.impact) {
                        content.graphics.lineStyle(1, MenuStyle.LINE);
                        content.graphics.moveTo(0, extent); content.graphics.lineTo(WIDTH - 18, extent);
                        content.graphics.lineStyle(); extent += 24;
                    }
                    line("WHAT YOU CAN DO", 21, MenuStyle.MUTED, true, 12);
                    line(row.nextSteps, CONFIG::largeText ? 30 : 27, MenuStyle.MUTED, false, 8);
                }
            }
            if (!preserve) position = 0;
            scroll(0);
        }

        private function line(text:String, size:Number, color:uint, label:Boolean, after:Number):void
        {
            var field:TextField = MenuStyle.field("", 0, extent, WIDTH - 18, 40, size, color, label);
            field.multiline = true; field.wordWrap = true;
            var format:TextFormat = field.defaultTextFormat; format.leading = label ? 3 : 8;
            field.defaultTextFormat = format; MenuStyle.setText(field, text);
            field.height = field.textHeight + 8; content.addChild(field);
            extent += field.height + after;
        }

        public function scroll(delta:Number):void
        {
            position = Math.max(0, Math.min(position + delta, Math.max(0, extent - HEIGHT)));
            content.y = -position;
            // A quiet position indicator only appears when text extends below the pane.
            graphics.clear(); graphics.beginFill(0, 0); graphics.drawRect(0, 0, WIDTH, HEIGHT); graphics.endFill();
            if (scrollable) {
                var thumb:Number = Math.max(24, HEIGHT * HEIGHT / extent);
                graphics.beginFill(MenuStyle.LINE); graphics.drawRect(WIDTH - 4, 0, 2, HEIGHT); graphics.endFill();
                graphics.beginFill(MenuStyle.MUTED);
                graphics.drawRect(WIDTH - 5, position / (extent - HEIGHT) * (HEIGHT - thumb), 4, thumb); graphics.endFill();
            }
        }

        private function wheel(event:MouseEvent):void { scroll(-event.delta * 36); event.stopPropagation(); }
    }
}
