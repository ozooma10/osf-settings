package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;
    import flash.text.TextField;
    import flash.text.TextFormat;

    public final class IssueDetails extends Sprite
    {
        private static const WIDTH:Number = MenuStyle.RIGHT - IssueStyle.DETAIL_X;
        private static const HEIGHT:Number = MenuStyle.LIST_BOTTOM - IssueStyle.TOP;
        private static const PAD:Number = 24;
        private static const TEXT_WIDTH:Number = WIDTH - PAD * 2 - 12;
        private static const VIEW_HEIGHT:Number = HEIGHT - PAD * 2;
        private var viewport:Sprite = new Sprite();
        private var content:Sprite = new Sprite();
        private var position:Number = 0;
        private var extent:Number = 0;
        private var selected:Object;

        public function IssueDetails()
        {
            name = "issueDetails";
            x = IssueStyle.DETAIL_X; y = IssueStyle.TOP;
            viewport.x = viewport.y = PAD;
            viewport.scrollRect = new Rectangle(0, 0, TEXT_WIDTH, VIEW_HEIGHT);
            viewport.addChild(content); addChild(viewport);
            // A transparent hit area lets the wheel work between paragraphs too.
            graphics.beginFill(0, 0); graphics.drawRect(0, 0, WIDTH, HEIGHT); graphics.endFill();
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
        }

        public function get scrollable():Boolean { return extent > VIEW_HEIGHT; }
        CONFIG::testHarness {
            public function testState():Object { return {position:position, extent:extent, height:VIEW_HEIGHT}; }
        }

        public function show(row:Object):void
        {
            if (selected == row) return;
            var preserve:Boolean = selected && row && selected.mod == row.mod && selected.id == row.id;
            selected = row;
            while (content.numChildren) content.removeChildAt(0);
            content.graphics.clear(); extent = 0;
            if (row) {
                line(row.modTitle + " / " + (row.severityLabel || tr(row.severity == "ERROR" ? "issues.error" : "issues.warning")),
                    MenuStyle.SMALL_SIZE + 1, IssueStyle.color(row.severity), true, 12);
                line(row.title, MenuStyle.DETAIL_TITLE_SIZE + 4, MenuStyle.WHITE, false, 22);
                if (row.reason) {
                    var reasonStart:Number = extent;
                    var accent:uint = IssueStyle.color(row.severity);
                    extent += 16;
                    line(tr("issues.reason"), MenuStyle.SMALL_SIZE, accent, true, 8, 20, 20);
                    line(row.reason, MenuStyle.DETAIL_BODY_SIZE + 6, MenuStyle.WHITE, false, 16, 20, 20);
                    content.graphics.beginFill(accent, 0.08);
                    content.graphics.drawRect(0, reasonStart, TEXT_WIDTH, extent - reasonStart);
                    content.graphics.endFill();
                    content.graphics.beginFill(accent);
                    content.graphics.drawRect(0, reasonStart, 4, extent - reasonStart);
                    content.graphics.endFill();
                    extent += 24;
                } else if (row.impact || row.nextSteps) rule();
                if (row.impact) {
                    line(tr("issues.impact"), MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true, 8);
                    line(row.impact, MenuStyle.DETAIL_BODY_SIZE + 2, MenuStyle.WHITE, false, 22);
                }
                if (row.nextSteps) {
                    if (row.impact) rule();
                    line(tr("issues.nextSteps"), MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true, 8);
                    var start:Number = extent;
                    line(row.nextSteps, MenuStyle.DETAIL_BODY_SIZE + 2, MenuStyle.WHITE, false, 8, 22);
                    content.graphics.beginFill(IssueStyle.WARNING, 0.8);
                    content.graphics.drawRect(0, start + 4, 4, extent - start - 12); content.graphics.endFill();
                }
            }
            if (!preserve) position = 0;
            scroll(0);
        }

        private function rule():void
        {
            content.graphics.lineStyle(1, MenuStyle.LINE);
            content.graphics.moveTo(0, extent); content.graphics.lineTo(TEXT_WIDTH, extent);
            content.graphics.lineStyle(); extent += 24;
        }

        private function line(text:String, size:Number, color:uint, label:Boolean, after:Number, inset:Number = 0, rightInset:Number = 0):void
        {
            var field:TextField = MenuStyle.field("", inset, extent, TEXT_WIDTH - inset - rightInset, 40, size, color, label);
            field.multiline = true; field.wordWrap = true;
            var format:TextFormat = field.defaultTextFormat; format.leading = label ? 2 : 6;
            field.defaultTextFormat = format; MenuStyle.setText(field, text);
            field.height = field.textHeight + 8; content.addChild(field);
            extent += field.height + after;
        }

        public function scroll(delta:Number):void
        {
            position = Math.max(0, Math.min(position + delta, Math.max(0, extent - VIEW_HEIGHT)));
            content.y = -position;
            // A quiet position indicator only appears when text extends below the pane.
            graphics.clear(); graphics.beginFill(0, 0); graphics.drawRect(0, 0, WIDTH, HEIGHT); graphics.endFill();
            if (selected) MenuDecoration.corners(graphics, 0, 0, WIDTH, HEIGHT, MenuStyle.WHITE);
            if (scrollable) {
                var thumb:Number = Math.max(24, VIEW_HEIGHT * VIEW_HEIGHT / extent);
                graphics.beginFill(MenuStyle.LINE); graphics.drawRect(WIDTH - 9, PAD, 2, VIEW_HEIGHT); graphics.endFill();
                graphics.beginFill(MenuStyle.MUTED);
                graphics.drawRect(WIDTH - 10, PAD + position / (extent - VIEW_HEIGHT) * (VIEW_HEIGHT - thumb), 4, thumb); graphics.endFill();
            }
        }

        private function wheel(event:MouseEvent):void { scroll(-event.delta * 36); event.stopPropagation(); }
    }
}
