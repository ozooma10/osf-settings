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

    // Reads problem, fix, then reference material folded under Details.
    public final class IssueDetails extends Sprite
    {
        private static const WIDTH:Number = MenuStyle.RIGHT - IssueStyle.DETAIL_X;
        private static const HEIGHT:Number = MenuStyle.LIST_BOTTOM - IssueStyle.TOP;
        private static const PAD:Number = 24;
        private static const TEXT_WIDTH:Number = WIDTH - PAD * 2 - 12;
        private static const VIEW_HEIGHT:Number = HEIGHT - PAD * 2;
        // The fix block insets its contents from the accent stripe.
        private static const INSET:Number = 20;
        private static const BUTTON_WIDTH:Number = TEXT_WIDTH - INSET * 2;
        private static const BUTTON_HEIGHT:Number = CONFIG::largeText ? 72 : 64;
        private var viewport:Sprite = new Sprite();
        private var content:Sprite = new Sprite();
        private var position:Number = 0;
        private var extent:Number = 0;
        private var selected:Object;
        private var openModPage:Function;
        private var makeConfirmHint:Function;
        private var detailsChanged:Function;
        private var copyDetails:Function;
        private var pressedModPage:Object;
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
        // Label and value pairs for the selected report; open state lasts while browsing reports.
        private var references:Array = [];
        private var expanded:Boolean = false;
        private var detailsTop:Number = 0;

        public function IssueDetails(openModPage:Function, makeConfirmHint:Function, detailsChanged:Function, copyDetails:Function)
        {
            this.openModPage = openModPage;
            this.makeConfirmHint = makeConfirmHint;
            this.detailsChanged = detailsChanged;
            this.copyDetails = copyDetails;
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
        public function get hasDetails():Boolean { return references.length > 0; }
        public function get detailsExpanded():Boolean { return expanded; }
        public function get atEnd():Boolean { return position >= extent - VIEW_HEIGHT - 1; }
        CONFIG::testHarness {
            public function testState():Object { return {position:position, extent:extent, height:VIEW_HEIGHT,
                hovered:hovered, pressed:mousePressed || confirmHeld, confirmVisible:confirmHint && confirmHint.visible,
                details:expanded}; }
        }

        public function show(row:Object):void
        {
            if (selected == row) return;
            var preserve:Boolean = selected && row && selected.mod == row.mod && selected.id == row.id;
            selected = row;
            build(preserve);
        }

        public function toggleDetails():void
        {
            if (!references.length) return;
            expanded = !expanded;
            build(true);
            // Bring opened details into view without hiding their heading; closing returns to the problem.
            scroll(expanded ? Math.max(0, Math.min(detailsTop - 40, extent - VIEW_HEIGHT)) - position : -position);
            detailsChanged();
        }

        private function build(preserve:Boolean):void
        {
            var row:Object = selected;
            cancelInteraction();
            pressedModPage = null;
            modPage = null; modPageText = null; confirmHint = null; hintBounds = null;
            while (content.numChildren) content.removeChildAt(0);
            content.graphics.clear(); extent = 0;
            references = row ? referenceRows(row) : [];
            if (row) {
                eyebrow(row);
                line(row.title, MenuStyle.DETAIL_TITLE_SIZE + 4, MenuStyle.WHITE, false, 14);
                if (row.impact) line(row.impact, MenuStyle.DETAIL_BODY_SIZE + 2, MenuStyle.WHITE, false, 24);
                else extent += 10;
                if (row.nextSteps || row.requiredVersion || row.nexusModId) fix(row);
                if (references.length) details();
            }
            if (!preserve) position = 0;
            scroll(0);
        }

        // Severity, then the mods the report affects.
        private function eyebrow(row:Object):void
        {
            var size:Number = MenuStyle.SMALL_SIZE + 1;
            var severity:TextField = textAt(IssueStyle.label(row), 0, extent, IssueStyle.color(row.severity), size);
            var left:Number = severity.width + 24;
            var mods:TextField = MenuStyle.field("", left, extent, TEXT_WIDTH - left, 40, size, MenuStyle.MUTED, true);
            // A narrow remainder would wrap every name, so long labels move the list to its own line.
            if (TEXT_WIDTH - left < 220) { mods.x = 0; mods.y = extent + severity.height; mods.width = TEXT_WIDTH; }
            else {
                content.graphics.lineStyle(1, MenuStyle.LINE);
                content.graphics.moveTo(left - 13, extent + 5); content.graphics.lineTo(left - 13, extent + severity.height - 5);
                content.graphics.lineStyle();
            }
            mods.multiline = mods.wordWrap = true;
            MenuStyle.setText(mods, IssueStyle.affected(row));
            mods.height = mods.textHeight + 6; content.addChild(mods);
            extent = mods.y + mods.height + 10;
        }

        private function fix(row:Object):void
        {
            var start:Number = extent;
            extent += 16;
            line(tr("issues.nextSteps"), MenuStyle.SMALL_SIZE, MenuStyle.ACCENT, true, 8, INSET, INSET);
            if (row.nextSteps) line(row.nextSteps, MenuStyle.DETAIL_BODY_SIZE + 2, MenuStyle.WHITE, false, 12, INSET, INSET);
            if (row.requiredVersion) versionChip(row);
            if (row.nexusModId) modPageButton();
            extent += 10;
            content.graphics.beginFill(MenuStyle.ACCENT, 0.07);
            content.graphics.drawRect(0, start, TEXT_WIDTH, extent - start); content.graphics.endFill();
            content.graphics.beginFill(MenuStyle.ACCENT);
            content.graphics.drawRect(0, start, 4, extent - start); content.graphics.endFill();
            extent += 28;
        }

        // The fix text names the version needed; the chip says what is installed.
        private function versionChip(row:Object):void
        {
            var installed:Boolean = Boolean(row.installedVersion);
            var chip:TextField = MenuStyle.field("", INSET + 12, extent + 5, BUTTON_WIDTH - 24, 40, MenuStyle.SMALL_SIZE,
                installed ? MenuStyle.WHITE : IssueStyle.ERROR, true);
            chip.multiline = chip.wordWrap = true;
            MenuStyle.setText(chip, installed ? tr("issues.installedVersion") + "  " + row.installedVersion : tr("issues.notInstalled"));
            chip.width = Math.min(BUTTON_WIDTH - 24, chip.textWidth + 8); chip.height = chip.textHeight + 6;
            content.addChild(chip);
            content.graphics.lineStyle(1, MenuStyle.LINE);
            content.graphics.drawRect(INSET, extent, chip.width + 24, chip.height + 10);
            content.graphics.lineStyle();
            extent += chip.height + 24;
        }

        private function details():void
        {
            rule();
            detailsTop = extent;
            var toggle:Sprite = new Sprite(); toggle.name = "issueDetailsToggle";
            toggle.buttonMode = true; toggle.mouseChildren = false; toggle.y = extent;
            var label:TextField = MenuStyle.field("", 28, 0, TEXT_WIDTH - 28, 40, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            MenuStyle.setText(label, tr("issues.details")); label.height = label.textHeight + 6;
            toggle.addChild(label);
            var middle:Number = label.height / 2;
            toggle.graphics.beginFill(0, 0); toggle.graphics.drawRect(0, -8, TEXT_WIDTH, label.height + 16); toggle.graphics.endFill();
            toggle.graphics.beginFill(MenuStyle.MUTED);
            if (expanded) { toggle.graphics.moveTo(3, middle - 4); toggle.graphics.lineTo(17, middle - 4); toggle.graphics.lineTo(10, middle + 4); }
            else { toggle.graphics.moveTo(6, middle - 7); toggle.graphics.lineTo(14, middle); toggle.graphics.lineTo(6, middle + 7); }
            toggle.graphics.endFill();
            // Like the mod page button, act on press: GFx can drop the final click.
            toggle.addEventListener(MouseEvent.MOUSE_DOWN, function(event:MouseEvent):void { toggleDetails(); });
            content.addChild(toggle); extent += label.height + 16;
            if (!expanded) return;
            for each (var entry:Array in references) {
                line(entry[0], MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true, 4);
                line(entry[1], MenuStyle.DETAIL_BODY_SIZE, MenuStyle.WHITE, false, 14);
            }
            copyButton();
        }

        // The footer's Copy details control, for the mouse.
        private function copyButton():void
        {
            var button:Sprite = new Sprite(); button.name = "issueCopyDetails";
            button.buttonMode = true; button.mouseChildren = false; button.y = extent + 4;
            var label:TextField = MenuStyle.field("", 18, 0, TEXT_WIDTH - 36, 40, MenuStyle.SMALL_SIZE, MenuStyle.WHITE, true);
            MenuStyle.setText(label, tr("buttons.copyDetails"));
            label.width = label.textWidth + 8; label.height = label.textHeight + 6; label.y = 9;
            button.addChild(label);
            var paint:Function = function(hover:Boolean):void {
                button.graphics.clear();
                button.graphics.beginFill(MenuStyle.WHITE, hover ? 0.12 : 0);
                button.graphics.lineStyle(1, hover ? MenuStyle.WHITE : MenuStyle.LINE);
                button.graphics.drawRect(0, 0, label.width + 36, label.height + 18);
                button.graphics.lineStyle(); button.graphics.endFill();
            };
            paint(false);
            button.addEventListener(MouseEvent.ROLL_OVER, function(event:MouseEvent):void { paint(true); });
            button.addEventListener(MouseEvent.ROLL_OUT, function(event:MouseEvent):void { paint(false); });
            // Act on press: GFx can drop the final click.
            button.addEventListener(MouseEvent.MOUSE_DOWN, function(event:MouseEvent):void { copyDetails(); });
            content.addChild(button); extent = button.y + button.height + 8;
        }

        private static function referenceRows(row:Object):Array
        {
            var rows:Array = [];
            // The eyebrow already names a mod that reports its own issue.
            if (row.modTitle && IssueStyle.affected(row) != row.modTitle) rows.push([tr("issues.reportedBy"), row.modTitle]);
            if (row.reason) rows.push([tr("issues.reason"), row.reason]);
            if (row.nexusModId) rows.push([tr("issues.modPage"), "nexusmods.com/starfield/mods/" + row.nexusModId]);
            if (row.note) rows.push([tr("issues.note"), row.note]);
            // Every report has an ID, so Details and its Copy control are always available.
            rows.push([tr("issues.issueId"), row.mod + "/" + row.id]);
            return rows;
        }

        // A prominent, clickable twin of the footer's Open mod page control.
        private function modPageButton():void
        {
            var button:Sprite = new Sprite(); button.name = "issueModPage"; modPage = button;
            button.buttonMode = true; button.mouseChildren = false; button.x = INSET; button.y = extent;
            modPageText = centered(tr("buttons.openModPage"), MenuStyle.VALUE_SIZE + 4, MenuStyle.INK, true);
            button.addChild(modPageText);
            confirmHint = makeConfirmHint(); confirmHint.name = "issueConfirmHint";
            button.addChild(confirmHint);
            button.addEventListener(MouseEvent.ROLL_OVER, modPageHover);
            button.addEventListener(MouseEvent.ROLL_OUT, modPageHover);
            // Like sidebar rows, activate on press: GFx can drop the final click.
            button.addEventListener(MouseEvent.MOUSE_DOWN, modPagePressed);
            button.addEventListener(MouseEvent.CLICK, modPageClicked);
            content.addChild(button); extent += BUTTON_HEIGHT + 4;
            paintButton(); layoutButton();
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
            modPage.graphics.drawRect(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT); modPage.graphics.endFill();
            if (actionFocused || hovered) {
                modPage.graphics.lineStyle(2, MenuStyle.INK, down ? 0.8 : 0.45);
                modPage.graphics.drawRect(3, 3, BUTTON_WIDTH - 6, BUTTON_HEIGHT - 6); modPage.graphics.lineStyle();
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
            modPageText.width = Math.min(BUTTON_WIDTH - hintWidth - 32, modPageText.textWidth + 8);
            MenuStyle.fit(modPageText, tr("buttons.openModPage"));
            modPageText.x = (BUTTON_WIDTH - modPageText.width - hintWidth) / 2;
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
            var field:TextField = MenuStyle.field("", 0, 0, BUTTON_WIDTH, 40, size, color, bold);
            MenuStyle.align(field, "center"); MenuStyle.setText(field, text);
            field.height = field.textHeight + 6;
            return field;
        }

        private function rule():void
        {
            content.graphics.lineStyle(1, MenuStyle.LINE);
            content.graphics.moveTo(0, extent); content.graphics.lineTo(TEXT_WIDTH, extent);
            content.graphics.lineStyle(); extent += 20;
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
            var wasAtEnd:Boolean = atEnd;
            position = Math.max(0, Math.min(position + delta, Math.max(0, extent - VIEW_HEIGHT)));
            // With details open, the footer's scroll control becomes Copy details at the end.
            if (expanded && atEnd != wasAtEnd) detailsChanged();
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

        // Controllers have one scroll button, so it returns to the top after the end.
        public function scrollOn():void
        {
            if (position >= extent - VIEW_HEIGHT - 1) scroll(-position);
            else scroll(240);
        }

        private function wheel(event:MouseEvent):void { scroll(-event.delta * 36); event.stopPropagation(); }
    }
}
