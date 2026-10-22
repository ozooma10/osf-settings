package
{
    import flash.display.Sprite;
    import flash.display.MovieClip;
    import flash.display.Stage;
    import flash.events.Event;
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
        private static const BUTTON_HEIGHT:Number = CONFIG::largeText ? 72 : 64;
        private var selected:Object;
        private var openModPage:Function;
        private var pressedModPage:Object;
        private var makeConfirmHint:Function;
        private var modPage:Sprite;
        private var modPageText:TextField;
        private var confirmHint:MovieClip;
        private var hovered:Boolean = false;
        private var mousePressed:Boolean = false;
        private var actionFocused:Boolean = false;
        private var confirmHeld:Boolean = false;
        private var releaseStage:Stage;
        // The hint geometry last laid out; null until the button is laid out once.
        private var hintBounds:Rectangle;
        private var hintFocused:Boolean;
        private var hintOffset:Number;

        public function IssueDetails(openModPage:Function, makeConfirmHint:Function)
        {
            this.openModPage = openModPage;
            this.makeConfirmHint = makeConfirmHint;
            name = "issueDetails";
            x = IssueStyle.DETAIL_X; y = IssueStyle.TOP;
            viewport.x = viewport.y = PAD;
            viewport.scrollRect = new Rectangle(0, 0, TEXT_WIDTH, VIEW_HEIGHT);
            viewport.addChild(content); addChild(viewport);
            // A transparent hit area lets the wheel work between paragraphs too.
            graphics.beginFill(0, 0); graphics.drawRect(0, 0, WIDTH, HEIGHT); graphics.endFill();
            addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
            addEventListener(Event.REMOVED_FROM_STAGE, cancelInteraction);
        }

        public function get scrollable():Boolean { return extent > VIEW_HEIGHT; }
        CONFIG::testHarness {
            public function testState():Object { return {position:position, extent:extent, height:VIEW_HEIGHT,
                hovered:hovered, pressed:mousePressed || confirmHeld, confirmVisible:confirmHint && confirmHint.visible}; }
        }

        public function show(row:Object):void
        {
            if (selected == row) return;
            var preserve:Boolean = selected && row && selected.mod == row.mod && selected.id == row.id;
            selected = row;
            cancelInteraction();
            pressedModPage = null;
            modPage = null; modPageText = null; confirmHint = null; hintBounds = null;
            while (content.numChildren) content.removeChildAt(0);
            content.graphics.clear(); extent = 0;
            if (row) {
                line(row.modTitle + " / " + IssueStyle.label(row), MenuStyle.SMALL_SIZE + 1, IssueStyle.color(row.severity), true, 12);
                line(row.title, MenuStyle.DETAIL_TITLE_SIZE + 4, MenuStyle.WHITE, false, 22);
                if (row.requiredVersion) versions(row);
                if (row.nexusModId) modPageButton(row.nexusModId);
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

        // Installed and required versions side by side, so the gap reads at a glance.
        private function versions(row:Object):void
        {
            var top:Number = extent, inset:Number = 20, big:Number = MenuStyle.DETAIL_TITLE_SIZE + 30;
            var installed:Boolean = Boolean(row.installedVersion);
            var haveLabel:TextField = textAt(tr("issues.installedVersion"), inset, top + 16, MenuStyle.MUTED);
            var have:TextField = textAt(installed ? row.installedVersion : tr("issues.notInstalled"), inset, 0,
                installed ? MenuStyle.MUTED : IssueStyle.ERROR, installed ? big : MenuStyle.DETAIL_TITLE_SIZE + 4);
            var arrow:Number = Math.max(haveLabel.x + haveLabel.width, have.x + have.width) + 18;
            var needLabel:TextField = textAt(tr("issues.requiredVersion"), arrow + 62, top + 16, IssueStyle.WARNING);
            var need:TextField = textAt(row.requiredVersion, needLabel.x, 0, IssueStyle.WARNING, big);
            // Align the numbers on their bottoms when "not installed" uses smaller text.
            var bottom:Number = haveLabel.y + haveLabel.height + Math.max(have.height, need.height);
            have.y = bottom - have.height; need.y = bottom - need.height;
            var height:Number = bottom + 14 - top;
            content.graphics.beginFill(0x0E1B23); content.graphics.drawRect(0, top, TEXT_WIDTH, height); content.graphics.endFill();
            var middle:Number = bottom - need.height / 2;
            content.graphics.lineStyle(2, MenuStyle.MUTED);
            content.graphics.moveTo(arrow, middle); content.graphics.lineTo(arrow + 42, middle);
            content.graphics.moveTo(arrow + 32, middle - 10); content.graphics.lineTo(arrow + 42, middle);
            content.graphics.lineTo(arrow + 32, middle + 10); content.graphics.lineStyle();
            extent += height + 20;
        }

        // A prominent, clickable twin of the footer's Open mod page control.
        private function modPageButton(nexusModId:uint):void
        {
            var button:Sprite = new Sprite(); button.name = "issueModPage"; modPage = button;
            button.buttonMode = true; button.mouseChildren = false; button.y = extent;
            modPageText = centered(tr("buttons.openModPage"), MenuStyle.VALUE_SIZE + 4, MenuStyle.INK, true);
            button.addChild(modPageText);
            confirmHint = makeConfirmHint(); confirmHint.name = "issueConfirmHint";
            button.addChild(confirmHint);
            button.addEventListener(MouseEvent.ROLL_OVER, modPageHover);
            button.addEventListener(MouseEvent.ROLL_OUT, modPageHover);
            // Like sidebar rows, activate on press: GFx can drop the final click.
            button.addEventListener(MouseEvent.MOUSE_DOWN, modPagePressed);
            button.addEventListener(MouseEvent.CLICK, modPageClicked);
            content.addChild(button); extent += BUTTON_HEIGHT + 8;
            paintButton(); layoutButton();
            var url:TextField = centered("nexusmods.com/starfield/mods/" + nexusModId, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, false);
            url.y = extent; content.addChild(url); extent += url.height + 18;
        }

        private function modPagePressed(event:MouseEvent):void
        {
            pressedModPage = event.currentTarget;
            hovered = mousePressed = true;
            releaseStage = stage;
            if (releaseStage) releaseStage.addEventListener(MouseEvent.MOUSE_UP, modPageReleased, true);
            paintButton(); layoutButton();
            openModPage();
        }

        private function modPageReleased(event:MouseEvent = null):void
        {
            if (releaseStage) releaseStage.removeEventListener(MouseEvent.MOUSE_UP, modPageReleased, true);
            releaseStage = null; mousePressed = false;
            paintButton(); layoutButton();
        }

        private function modPageHover(event:MouseEvent):void
        {
            hovered = event.type == MouseEvent.ROLL_OVER;
            paintButton(); layoutButton();
        }

        public function cancelInteraction(event:Event = null):void
        {
            hovered = confirmHeld = false;
            modPageReleased();
        }

        public function updateInteraction(focused:Boolean, held:Boolean):void
        {
            var changed:Boolean = actionFocused != focused || confirmHeld != (focused && held);
            actionFocused = focused; confirmHeld = focused && held;
            if (changed) paintButton();
            // Native hints can resize after a device switch or a binding update.
            layoutButton();
        }

        private function paintButton():void
        {
            if (!modPage) return;
            var down:Boolean = mousePressed && hovered || confirmHeld;
            modPage.graphics.clear();
            modPage.graphics.beginFill(down ? 0xC49145 : hovered ? MenuStyle.WHITE : IssueStyle.WARNING);
            modPage.graphics.drawRect(0, 0, TEXT_WIDTH, BUTTON_HEIGHT); modPage.graphics.endFill();
            if (actionFocused || hovered) {
                modPage.graphics.lineStyle(2, MenuStyle.INK, down ? 0.8 : 0.45);
                modPage.graphics.drawRect(3, 3, TEXT_WIDTH - 6, BUTTON_HEIGHT - 6); modPage.graphics.lineStyle();
            }
        }

        private function layoutButton():void
        {
            if (!modPage || !confirmHint) return;
            confirmHint.visible = actionFocused;
            var bounds:Rectangle = confirmHint.getBounds(confirmHint);
            var offset:Number = mousePressed && hovered || confirmHeld ? 2 : 0;
            if (hintBounds && hintBounds.equals(bounds) && hintFocused == actionFocused && hintOffset == offset) return;
            hintBounds = bounds; hintFocused = actionFocused; hintOffset = offset;
            var hintWidth:Number = actionFocused ? bounds.width + 18 : 0;
            MenuStyle.setText(modPageText, tr("buttons.openModPage"));
            modPageText.width = Math.min(TEXT_WIDTH - hintWidth - 32, modPageText.textWidth + 8);
            MenuStyle.fit(modPageText, tr("buttons.openModPage"));
            modPageText.x = (TEXT_WIDTH - modPageText.width - hintWidth) / 2;
            modPageText.y = (BUTTON_HEIGHT - modPageText.height) / 2 + 2 + offset;
            confirmHint.x = modPageText.x + modPageText.width + 18 - bounds.x;
            confirmHint.y = (BUTTON_HEIGHT - bounds.height) / 2 - bounds.y + offset;
        }

        private function modPageClicked(event:MouseEvent):void
        {
            if (pressedModPage == event.currentTarget) { pressedModPage = null; return; }
            openModPage();
        }

        private function textAt(text:String, x:Number, y:Number, color:uint, size:Number = 0):TextField
        {
            var field:TextField = MenuStyle.field("", x, y, TEXT_WIDTH, 40, size || MenuStyle.SMALL_SIZE, color, true);
            MenuStyle.setText(field, text);
            field.width = field.textWidth + 8; field.height = field.textHeight + 6;
            content.addChild(field);
            return field;
        }

        private function centered(text:String, size:Number, color:uint, bold:Boolean):TextField
        {
            var field:TextField = MenuStyle.field("", 0, 0, TEXT_WIDTH, 40, size, color, bold);
            MenuStyle.align(field, "center"); MenuStyle.setText(field, text);
            field.height = field.textHeight + 6;
            return field;
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
