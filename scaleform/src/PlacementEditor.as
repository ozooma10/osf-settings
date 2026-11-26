package
{
    import flash.display.Graphics;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.FocusEvent;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.text.TextField;
    import flash.ui.Keyboard;

    // Modal placement editor: a scaled copy of the screen with the rectangle on it. Drag the
    // rectangle to move it and its handles to resize it; guides show the margin to each edge.
    // Arrows move (or resize, after X), [ ] or the shoulders shrink and grow, B or Y loads the
    // defaults. Nothing is saved until Save writes every bound setting.
    public final class PlacementEditor extends Sprite
    {
        public var row:Object;
        private static const LARGE:Boolean = CONFIG::largeText;
        private static const BOX_X:Number = 32;
        private static const BOX_Y:Number = 32;
        private static const BOX_WIDTH:Number = 1920 - 2 * BOX_X;
        private static const BOX_HEIGHT:Number = 1080 - 2 * BOX_Y;
        private static const PAD:Number = 32;
        private static const PANEL_WIDTH:Number = LARGE ? 440 : 400;
        private static const PANEL_X:Number = BOX_X + BOX_WIDTH - PAD - PANEL_WIDTH;
        private static const CONTENT_TOP:Number = BOX_Y + (LARGE ? 124 : 112);
        private static const BUTTON_WIDTH:Number = LARGE ? 210 : 190;
        private static const BUTTON_HEIGHT:Number = LARGE ? 56 : 50;
        private static const BUTTON_TOP:Number = BOX_Y + BOX_HEIGHT - PAD - BUTTON_HEIGHT;
        private static const AREA_LEFT:Number = BOX_X + PAD;
        private static const AREA_WIDTH:Number = PANEL_X - 40 - AREA_LEFT;
        private static const AREA_HEIGHT:Number = BUTTON_TOP - 28 - CONTENT_TOP;
        private static const HANDLE:Number = 14;
        // Snapping reach, in display pixels.
        private static const SNAP:Number = 10;
        private static const LINE_PITCH:Number = MenuStyle.VALUE_SIZE + 14;
        private static const GRID:uint = 0x1A2C35;
        // The game events the editor acts on; it declines the rest, so mouse buttons reach the movie.
        private static const NAMED_EVENTS:Object = {Accept:true, Cancel:true, Up:true, Down:true, Left:true, Right:true,
            LShoulder:true, RShoulder:true, XButton:true, YButton:true};
        private static const HANDLES:Array = ["nw", "n", "ne", "e", "se", "s", "sw", "w"];
        private static const READOUTS:Array = ["left", "top", "right", "bottom", "width", "height", "scale"];

        private var bridge:Object;
        private var finished:Function;
        private var heading:TextField;
        private var subtitle:TextField;
        private var canvas:Sprite = new Sprite();
        private var scenery:Sprite = new Sprite();
        private var guides:Sprite = new Sprite();
        private var widget:Sprite = new Sprite();
        private var handles:Object = {};
        private var widgetTitle:TextField;
        private var widgetSize:TextField;
        private var guideLabels:Object = {};
        private var siblingLabels:Array = [];
        private var modeLabel:TextField;
        private var modeValue:TextField;
        private var modeHint:TextField;
        private var shapeLabel:TextField;
        private var readouts:Object = {};
        private var savesLabel:TextField;
        private var saves:Array = [];
        private var feedback:TextField;
        private var controls:TextField;
        private var buttons:Array = [];

        private var siblings:Array = [];
        private var scale:Number = 1;
        private var canvasWidth:Number = 0;
        private var canvasHeight:Number = 0;
        private var rect:Object;
        // A save that failed part-way has still written the settings before the failure.
        private var wrote:Boolean;
        private var aspect:Number = 1;
        private var resizing:Boolean;
        private var acceptBlocked:Boolean;
        private var acceptDown:Boolean;
        private var cancelDown:Boolean;
        private var holdX:int;
        private var holdY:int;
        private var holdFramesX:int;
        private var holdFramesY:int;
        private var dragging:String = "";
        private var dragStart:Object;
        private var pointerX:Number = 0;
        private var pointerY:Number = 0;
        private var snapX:Number = NaN;
        private var snapY:Number = NaN;
        private var mouseDownButton:Object;

        public function PlacementEditor(code:Object, onFinished:Function)
        {
            bridge = code; finished = onFinished;
            graphics.beginFill(0, 0.85); graphics.drawRect(-4096, -4096, 10000, 10000); graphics.endFill();
            graphics.beginFill(MenuStyle.ROW); graphics.lineStyle(1, MenuStyle.LINE);
            graphics.drawRect(BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT); graphics.endFill(); graphics.lineStyle();
            MenuDecoration.corners(graphics, BOX_X + 10, BOX_Y + 10, BOX_WIDTH - 20, BOX_HEIGHT - 20);
            var inner:Number = BOX_WIDTH - 2 * PAD;
            heading = text("", AREA_LEFT, BOX_Y + 24, inner, MenuStyle.DETAIL_TITLE_SIZE + 20, MenuStyle.DETAIL_TITLE_SIZE + 3, MenuStyle.WHITE, true);
            subtitle = text("", AREA_LEFT, BOX_Y + 30 + MenuStyle.DETAIL_TITLE_SIZE + 12, inner, MenuStyle.DETAIL_BODY_SIZE + 14, MenuStyle.DETAIL_BODY_SIZE, MenuStyle.MUTED);

            addChild(canvas);
            scenery.mouseEnabled = scenery.mouseChildren = false; canvas.addChild(scenery);
            for (var i:int = 0; i < 8; ++i) {
                var sibling:TextField = MenuStyle.field("", 0, 0, 240, MenuStyle.SMALL_SIZE + 10, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
                sibling.visible = false; scenery.addChild(sibling); siblingLabels.push(sibling);
            }
            widget.name = "widget"; widget.buttonMode = true; widget.mouseChildren = false;
            widget.addEventListener(MouseEvent.MOUSE_DOWN, beginDrag);
            canvas.addChild(widget);
            widgetTitle = MenuStyle.field("", 0, 0, 300, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true);
            widgetSize = MenuStyle.field("", 0, 0, 300, MenuStyle.SMALL_SIZE + 10, MenuStyle.SMALL_SIZE, MenuStyle.WHITE, true);
            MenuStyle.align(widgetTitle, "center"); MenuStyle.align(widgetSize, "center");
            widget.addChild(widgetTitle); widget.addChild(widgetSize);
            guides.mouseEnabled = guides.mouseChildren = false; canvas.addChild(guides);
            for each (var side:String in ["left", "top", "right", "bottom"]) {
                var label:TextField = MenuStyle.field("", 0, 0, 120, MenuStyle.SMALL_SIZE + 10, MenuStyle.SMALL_SIZE, MenuStyle.WHITE, true);
                MenuStyle.align(label, "center"); guides.addChild(label); guideLabels[side] = label;
            }
            for each (var id:String in HANDLES) {
                var handle:Sprite = new Sprite(); handle.name = id; handle.buttonMode = true;
                handle.graphics.lineStyle(2, MenuStyle.INK); handle.graphics.beginFill(MenuStyle.WHITE);
                handle.graphics.drawRect(-HANDLE / 2, -HANDLE / 2, HANDLE, HANDLE); handle.graphics.endFill();
                handle.addEventListener(MouseEvent.MOUSE_DOWN, beginDrag);
                canvas.addChild(handle); handles[id] = handle;
            }
            canvas.addEventListener(MouseEvent.MOUSE_WHEEL, wheel);

            // The readout panel: what the arrows do, the rectangle, then the settings a save writes.
            var x:Number = PANEL_X, top:Number = CONTENT_TOP;
            modeLabel = text(tr("placement.arrows"), x, top, PANEL_WIDTH, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            modeValue = text("", x, top - 2, PANEL_WIDTH, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true);
            MenuStyle.align(modeValue, "right");
            modeHint = text("", x, top + LINE_PITCH - 6, PANEL_WIDTH, MenuStyle.SMALL_SIZE + 10, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            MenuStyle.align(modeHint, "right");
            shapeLabel = text(tr("placement.rectangle"), x, 0, PANEL_WIDTH, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            var titles:Object = {left:tr("placement.left"), top:tr("placement.top"), right:tr("placement.right"), bottom:tr("placement.bottom"),
                width:tr("placement.width"), height:tr("placement.height"), scale:tr("placement.scale")};
            for each (var role:String in READOUTS) {
                var readout:Object = {label:text(titles[role], x, 0, PANEL_WIDTH / 2, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.MUTED, true),
                    value:text("", x + PANEL_WIDTH / 2, 0, PANEL_WIDTH / 2, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true)};
                MenuStyle.align(readout.value, "right");
                readouts[role] = readout;
            }
            savesLabel = text(tr("placement.saves"), x, 0, PANEL_WIDTH, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            for (i = 0; i < Placement.ROLES.length; ++i) {
                var line:Object = {label:text("", x, 0, PANEL_WIDTH - 110, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.MUTED, true),
                    value:text("", x + PANEL_WIDTH - 110, 0, 110, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true)};
                MenuStyle.align(line.value, "right");
                saves.push(line);
            }
            feedback = text("", x, 0, PANEL_WIDTH, MenuStyle.SMALL_SIZE * 2 + 18, MenuStyle.SMALL_SIZE, MenuStyle.ACCENT, true);
            feedback.multiline = feedback.wordWrap = true;

            var buttonsLeft:Number = BOX_X + BOX_WIDTH - PAD - 3 * BUTTON_WIDTH - 2 * 16;
            controls = text("", AREA_LEFT, BUTTON_TOP - 4, buttonsLeft - AREA_LEFT - 24, BUTTON_HEIGHT + 16, MenuStyle.SMALL_SIZE, MenuStyle.MUTED);
            controls.multiline = controls.wordWrap = true;
            var names:Array = ["defaults", "cancel", "save"], captions:Array = [tr("placement.defaults"), tr("buttons.cancel"), tr("buttons.save")];
            for (i = 0; i < 3; ++i) {
                var button:Sprite = new Sprite(); button.name = names[i];
                button.buttonMode = true; button.mouseChildren = false;
                button.x = buttonsLeft + i * (BUTTON_WIDTH + 16); button.y = BUTTON_TOP;
                var primary:Boolean = i == 2;
                var caption:TextField = MenuStyle.field(captions[i], 0, (BUTTON_HEIGHT - MenuStyle.DETAIL_BODY_SIZE) / 2 - 4,
                    BUTTON_WIDTH, MenuStyle.DETAIL_BODY_SIZE + 14, MenuStyle.DETAIL_BODY_SIZE, primary ? MenuStyle.INK : MenuStyle.WHITE, true);
                MenuStyle.align(caption, "center");
                button.addChild(caption);
                button.graphics.beginFill(primary ? MenuStyle.WHITE : MenuStyle.INK); button.graphics.lineStyle(1, MenuStyle.LINE);
                button.graphics.drawRect(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT); button.graphics.endFill();
                button.addEventListener(MouseEvent.MOUSE_DOWN, buttonPressed);
                button.addEventListener(MouseEvent.CLICK, buttonClicked);
                buttons.push(button); addChild(button);
            }
            visible = false;
        }

        private function text(value:String, x:Number, y:Number, width:Number, height:Number, size:Number, color:uint, label:Boolean = false):TextField
        {
            var result:TextField = MenuStyle.field(value, x, y, width, height, size, color, label);
            addChild(result); return result;
        }

        // others: the page's other placements on the same screen, drawn for reference.
        // held: Accept was already down when the editor opened, so its release must not save.
        public function open(value:Object, others:Array, held:Boolean):void
        {
            row = value;
            siblings = others || [];
            var W:Number = Placement.screenWidth(row), H:Number = Placement.screenHeight(row);
            scale = Math.min(AREA_WIDTH / W, AREA_HEIGHT / H);
            canvasWidth = Math.round(W * scale); canvasHeight = Math.round(H * scale);
            canvas.x = Math.round(AREA_LEFT + (AREA_WIDTH - canvasWidth) / 2);
            canvas.y = Math.round(CONTENT_TOP + (AREA_HEIGHT - canvasHeight) / 2);
            rect = copy(row.rect || Placement.rect(row, Placement.current(row)));
            aspect = rect.width / rect.height;
            resizing = false; wrote = false;
            acceptBlocked = held; acceptDown = cancelDown = false;
            holdX = holdY = 0; dragging = ""; mouseDownButton = null; snapX = snapY = NaN;
            MenuStyle.fit(heading, String(row.title).toUpperCase());
            MenuStyle.fit(subtitle, tr("placement.subtitle", {width:Placement.number(W), height:Placement.number(H)}));
            MenuStyle.setText(feedback, "");
            drawScenery();
            layoutPanel();
            visible = true;
            if (stage) {
                stage.addEventListener(FocusEvent.KEY_FOCUS_CHANGE, keepFocus, true);
                stage.focus = null;
            }
            addEventListener(Event.ENTER_FRAME, advance);
            draw();
        }

        public function cancel():void { if (visible) finish(wrote); }

        // Leaves without reporting, for menu teardown.
        public function close():void
        {
            if (!visible) return;
            endDrag();
            removeEventListener(Event.ENTER_FRAME, advance);
            if (stage) stage.removeEventListener(FocusEvent.KEY_FOCUS_CHANGE, keepFocus, true);
            visible = false; row = null; siblings = []; holdX = holdY = 0; acceptDown = cancelDown = false; mouseDownButton = null;
        }

        private function finish(saved:Boolean):void
        {
            close(); finished(saved);
        }

        private function save():void
        {
            if (!visible || dragging) return;
            var next:Object = Placement.values(row, rect), saved:Object = Placement.current(row);
            var changed:Array = [];
            for each (var role:String in Placement.ROLES) {
                if (!next.hasOwnProperty(role) || next[role] == saved[role]) continue;
                var span:Object = Placement.range(row, role);
                if (span && (next[role] < span.low || next[role] > span.high)) {
                    showError(tr("placement.outOfRange", {setting:String(row.bound[role].title)})); return;
                }
                changed.push(role);
            }
            if (!changed.length) { finish(wrote); return; }
            for each (role in changed) {
                var target:Object = row.bound[role];
                var result:Object = target.type == "int" ? bridge.setInt(target.mod, target.key, String(next[role])) :
                    bridge.setFloat(target.mod, target.key, Number(next[role]));
                if (!result || !result.ok) {
                    showError(result && result.error ? String(result.error) : tr("errors.save"));
                    return;
                }
                wrote = true;
                target.value = target.type == "int" ? String(next[role]) : next[role];
            }
            finish(true);
        }

        // Named game events and raw keys share this path; raw keys arrive only when native declines a named event.
        public function userEvent(name:String, pressed:Boolean, event:KeyboardEvent = null):Boolean
        {
            if (!visible) return false;
            if (event) {
                // The menu's keyboard shortcuts for reset and clear stand in for the face buttons.
                if (event.keyCode == Keyboard.B) name = "YButton";
                else if (event.keyCode == Keyboard.X) name = "XButton";
                event.stopImmediatePropagation(); if (pressed) event.preventDefault();
            } else if (!NAMED_EVENTS[name]) {
                // A mouse button arrives as another named event, and only a declined one reaches the movie as a click.
                return false;
            }
            if (name == "Accept") {
                if (pressed) { if (!acceptBlocked) acceptDown = true; }
                else {
                    var act:Boolean = !acceptBlocked && acceptDown;
                    acceptBlocked = acceptDown = false;
                    if (act) save();
                }
            } else if (name == "Cancel") {
                if (pressed) cancelDown = true;
                else if (cancelDown) { cancelDown = false; cancel(); }
            } else if (name == "Left" || name == "Right") {
                var across:int = name == "Left" ? -1 : 1;
                if (pressed) {
                    // Keyboard auto-repeat sends more presses; the frame timer owns repetition.
                    if (holdX != across) { holdX = across; holdFramesX = 0; nudge(across, 0, 1); }
                } else if (holdX == across) holdX = 0;
            } else if (name == "Up" || name == "Down") {
                var down:int = name == "Up" ? -1 : 1;
                if (pressed) {
                    if (holdY != down) { holdY = down; holdFramesY = 0; nudge(0, down, 1); }
                } else if (holdY == down) holdY = 0;
            } else if (pressed && (name == "LShoulder" || name == "RShoulder")) {
                grow(name == "LShoulder" ? 1 / 1.05 : 1.05);
            } else if (pressed && name == "XButton") {
                if (canResize()) { resizing = !resizing; holdX = holdY = 0; draw(); }
            } else if (pressed && name == "YButton") {
                loadDefaults();
            }
            return true;
        }

        private function advance(event:Event):void
        {
            if (dragging) return;
            // About a third of a second before repeating, then faster the longer it is held.
            if (holdX && ++holdFramesX > 20 && holdFramesX % 2 == 0) nudge(holdX, 0, speed(holdFramesX));
            if (holdY && ++holdFramesY > 20 && holdFramesY % 2 == 0) nudge(0, holdY, speed(holdFramesY));
        }
        private static function speed(frames:int):Number { return frames > 120 ? 16 : frames > 60 ? 4 : 1; }

        private function canResize():Boolean { return Placement.sizable(row, true) || Placement.sizable(row, false); }

        // Arrows move, or resize: Right widens and Up heightens.
        private function nudge(dx:int, dy:int, size:Number):void
        {
            if (!resizing) { moveTo(rect.left + dx * size, rect.top + dy * size, false); return; }
            if (Placement.proportional(row)) {
                var delta:Number = (dx != 0 ? dx : -dy) * size * sizeQuantum();
                resizeTo(rect.width + delta, (rect.width + delta) / aspect);
            } else {
                resizeTo(rect.width + dx * size * sizeQuantum(), rect.height - dy * size * sizeQuantum());
            }
        }
        private function grow(factor:Number):void
        {
            if (!canResize()) return;
            var width:Number = rect.width * factor, height:Number = rect.height * factor;
            // Make sure a step survives rounding to the scale setting's precision.
            var quantum:Number = sizeQuantum();
            if (Math.abs(width - rect.width) < quantum) width = rect.width + (factor > 1 ? quantum : -quantum);
            if (Placement.proportional(row)) height = width / aspect;
            else if (Math.abs(height - rect.height) < quantum) height = rect.height + (factor > 1 ? quantum : -quantum);
            resizeTo(width, height);
        }
        // The smallest resize that saving keeps: one unit, or one step of the scale's precision.
        private function sizeQuantum():Number
        {
            var target:Object = row.bound.scale;
            if (!target) return 1;
            var decimals:int = int(target.decimals) >= 0 ? int(target.decimals) : 3;
            return Math.max(0.5, Number(row.sizeWidth) * Math.pow(10, -Math.min(decimals, 6)));
        }

        private function loadDefaults():void
        {
            var defaults:Object = copy(row.defaultRect || Placement.rect(row, Placement.defaults(row)));
            if (Placement.proportional(row)) aspect = defaults.width / defaults.height;
            apply(defaults);
        }

        private function moveTo(left:Number, top:Number, snapping:Boolean):void
        {
            var next:Object = {left:Placement.movable(row, true) ? left : rect.left, top:Placement.movable(row, false) ? top : rect.top,
                width:rect.width, height:rect.height};
            snapX = snapY = NaN;
            if (snapping) {
                var horizontal:Object = snap([next.left, next.left + next.width / 2, next.left + next.width], targets(true));
                if (horizontal && Placement.movable(row, true)) { next.left += horizontal.delta; snapX = horizontal.at; }
                var vertical:Object = snap([next.top, next.top + next.height / 2, next.top + next.height], targets(false));
                if (vertical && Placement.movable(row, false)) { next.top += vertical.delta; snapY = vertical.at; }
            }
            apply(Placement.confine(row, next));
        }

        // Resizes about the point the mod positions the rectangle by, within every limit.
        private function resizeTo(width:Number, height:Number):void
        {
            var limits:Object = Placement.sizeLimits(row);
            if (!Placement.sizable(row, true)) width = rect.width;
            if (!Placement.sizable(row, false)) height = rect.height;
            if (Placement.proportional(row)) {
                var low:Number = Math.max(limits.minWidth, limits.minHeight * aspect);
                var high:Number = Math.min(limits.maxWidth, limits.maxHeight * aspect);
                width = Math.max(low, Math.min(high, width)); height = width / aspect;
            } else {
                width = Math.max(limits.minWidth, Math.min(limits.maxWidth, width));
                height = Math.max(limits.minHeight, Math.min(limits.maxHeight, height));
            }
            if (!Placement.fits(row, width, height)) return;
            var ax:Number = Placement.anchor(row, true), ay:Number = Placement.anchor(row, false);
            snapX = snapY = NaN;
            apply(Placement.confine(row, {left:rect.left + ax * (rect.width - width), top:rect.top + ay * (rect.height - height), width:width, height:height}));
        }

        // A handle drag keeps the opposite edge or corner where it was.
        private function resizeFrom(handle:String, dx:Number, dy:Number, snapping:Boolean):void
        {
            var start:Object = dragStart, W:Number = Placement.screenWidth(row), H:Number = Placement.screenHeight(row);
            var hx:int = handle.indexOf("w") >= 0 ? -1 : handle.indexOf("e") >= 0 ? 1 : 0;
            var hy:int = handle.indexOf("n") >= 0 ? -1 : handle.indexOf("s") >= 0 ? 1 : 0;
            var limits:Object = Placement.sizeLimits(row);
            var width:Number, height:Number;
            snapX = snapY = NaN;
            if (Placement.proportional(row)) {
                // Corners follow the pointer's average stretch; the size keeps its proportions.
                var ratio:Number = ((start.width + hx * dx) / start.width + (start.height + hy * dy) / start.height) / 2;
                var room:Number = Math.min(hx < 0 ? start.left + start.width : W - start.left, hy < 0 ? (start.top + start.height) * aspect : (H - start.top) * aspect);
                var high:Number = Math.min(limits.maxWidth, limits.maxHeight * aspect, room);
                width = Math.max(Math.max(limits.minWidth, limits.minHeight * aspect), Math.min(high, start.width * ratio));
                height = width / aspect;
            } else {
                var left:Number = start.left, top:Number = start.top, right:Number = start.left + start.width, bottom:Number = start.top + start.height;
                if (hx < 0) left = Math.max(0, left + dx); else if (hx > 0) right = Math.min(W, right + dx);
                if (hy < 0) top = Math.max(0, top + dy); else if (hy > 0) bottom = Math.min(H, bottom + dy);
                if (snapping) {
                    var edge:Object = hx ? snap([hx < 0 ? left : right], targets(true)) : null;
                    if (edge) { if (hx < 0) left += edge.delta; else right += edge.delta; snapX = edge.at; }
                    edge = hy ? snap([hy < 0 ? top : bottom], targets(false)) : null;
                    if (edge) { if (hy < 0) top += edge.delta; else bottom += edge.delta; snapY = edge.at; }
                }
                width = Placement.sizable(row, true) ? Math.max(limits.minWidth, Math.min(limits.maxWidth, right - left)) : start.width;
                height = Placement.sizable(row, false) ? Math.max(limits.minHeight, Math.min(limits.maxHeight, bottom - top)) : start.height;
            }
            if (!Placement.fits(row, width, height)) return;
            var next:Object = {left:hx < 0 ? start.left + start.width - width : start.left, top:hy < 0 ? start.top + start.height - height : start.top,
                width:width, height:height};
            apply(Placement.confine(row, next));
        }

        // Snapping lines: the screen's edges and centre, then the other placements' edges and centres.
        private function targets(horizontal:Boolean):Array
        {
            var extent:Number = horizontal ? Placement.screenWidth(row) : Placement.screenHeight(row);
            var result:Array = [0, extent / 2, extent];
            for each (var other:Object in siblings) {
                var r:Object = other.rect;
                if (!r) continue;
                var start:Number = horizontal ? r.left : r.top, size:Number = horizontal ? r.width : r.height;
                result.push(start, start + size / 2, start + size);
            }
            return result;
        }
        // The closest candidate within reach of a target, as {delta, at}, or null.
        private function snap(candidates:Array, lines:Array):Object
        {
            var reach:Number = SNAP / scale, best:Object = null;
            for each (var candidate:Number in candidates) for each (var line:Number in lines) {
                var delta:Number = line - candidate;
                if (Math.abs(delta) <= reach && (!best || Math.abs(delta) < Math.abs(best.delta))) best = {delta:delta, at:line};
            }
            return best;
        }

        private function apply(next:Object):void
        {
            rect = Placement.settle(row, next);
            MenuStyle.setText(feedback, "");
            draw();
        }

        private function showError(message:String):void { MenuStyle.setText(feedback, message); }

        // Tab must not move focus out of the modal.
        private function keepFocus(event:FocusEvent):void { if (visible) event.preventDefault(); }

        private function beginDrag(event:MouseEvent):void
        {
            event.stopPropagation();
            if (!row || dragging) return;
            var name:String = Sprite(event.currentTarget).name;
            dragging = name == "widget" ? "move" : name;
            dragStart = copy(rect); pointerX = canvas.mouseX; pointerY = canvas.mouseY;
            holdX = holdY = 0;
            if (stage) {
                stage.addEventListener(MouseEvent.MOUSE_MOVE, dragMoved);
                stage.addEventListener(MouseEvent.MOUSE_UP, endDrag);
            }
            draw();
        }
        private function dragMoved(event:MouseEvent = null):void
        {
            if (!dragging) return;
            var dx:Number = (canvas.mouseX - pointerX) / scale, dy:Number = (canvas.mouseY - pointerY) / scale;
            // Shift places freely, without snapping.
            var snapping:Boolean = !(event && event.shiftKey);
            if (dragging == "move") moveTo(dragStart.left + dx, dragStart.top + dy, snapping);
            else resizeFrom(dragging, dx, dy, snapping);
        }
        private function endDrag(event:MouseEvent = null):void
        {
            var wasDragging:Boolean = Boolean(dragging);
            dragging = "";
            snapX = snapY = NaN;
            if (stage) {
                stage.removeEventListener(MouseEvent.MOUSE_MOVE, dragMoved);
                stage.removeEventListener(MouseEvent.MOUSE_UP, endDrag);
            }
            if (wasDragging && visible) draw();
        }
        private function wheel(event:MouseEvent):void
        {
            event.stopPropagation();
            if (!event.delta || dragging) return;
            grow(event.delta > 0 ? 1.02 : 1 / 1.02);
        }
        private function buttonPressed(event:MouseEvent):void { mouseDownButton = event.currentTarget; event.stopPropagation(); }
        private function buttonClicked(event:MouseEvent):void
        {
            event.stopPropagation();
            if (mouseDownButton != event.currentTarget) return;
            mouseDownButton = null;
            var name:String = Sprite(event.currentTarget).name;
            if (name == "save") save(); else if (name == "defaults") loadDefaults(); else cancel();
        }

        private static function copy(r:Object):Object { return {left:r.left, top:r.top, width:r.width, height:r.height}; }

        // The screen, its grid and centre lines, and the page's other placements.
        private function drawScenery():void
        {
            // The canvas itself takes the wheel anywhere on the screen; the scenery ignores the mouse.
            canvas.graphics.clear(); canvas.graphics.beginFill(0, 0); canvas.graphics.drawRect(0, 0, canvasWidth, canvasHeight); canvas.graphics.endFill();
            var g:Graphics = scenery.graphics; g.clear();
            g.beginFill(MenuStyle.INK); g.drawRect(0, 0, canvasWidth, canvasHeight); g.endFill();
            var W:Number = Placement.screenWidth(row), H:Number = Placement.screenHeight(row);
            // A grid of roughly square cells, sixteen across a 16:9 screen.
            var cell:Number = W / 16;
            g.lineStyle(1, GRID);
            for (var x:Number = cell; x < W - cell / 2; x += cell) { g.moveTo(x * scale, 0); g.lineTo(x * scale, canvasHeight); }
            for (var y:Number = cell; y < H - cell / 2; y += cell) { g.moveTo(0, y * scale); g.lineTo(canvasWidth, y * scale); }
            g.lineStyle(1, MenuStyle.LINE);
            g.moveTo(canvasWidth / 2, 0); g.lineTo(canvasWidth / 2, canvasHeight);
            g.moveTo(0, canvasHeight / 2); g.lineTo(canvasWidth, canvasHeight / 2);
            for (var i:int = 0; i < siblingLabels.length; ++i) siblingLabels[i].visible = false;
            i = 0;
            for each (var other:Object in siblings) {
                var r:Object = other.rect;
                if (!r) continue;
                var box:Object = clip(r);
                if (!box) continue;
                g.lineStyle(1, MenuStyle.MUTED, 0.6); g.beginFill(MenuStyle.MUTED, 0.08);
                g.drawRect(box.x, box.y, box.width, box.height); g.endFill();
                if (i < siblingLabels.length && box.width > 40) {
                    var label:TextField = siblingLabels[i++];
                    label.width = Math.max(40, box.width - 8);
                    label.x = box.x + 4; label.y = box.y + 2;
                    MenuStyle.fit(label, String(other.title));
                    label.visible = true;
                }
            }
            g.lineStyle(2, MenuStyle.LINE); g.drawRect(0, 0, canvasWidth, canvasHeight); g.lineStyle();
        }

        // The part of a rectangle inside the screen, in display pixels, or null.
        private function clip(r:Object):Object
        {
            var left:Number = Math.max(0, r.left * scale), top:Number = Math.max(0, r.top * scale);
            var right:Number = Math.min(canvasWidth, (r.left + r.width) * scale), bottom:Number = Math.min(canvasHeight, (r.top + r.height) * scale);
            if (right - left < 1 || bottom - top < 1) return null;
            return {x:left, y:top, width:right - left, height:bottom - top};
        }

        private function draw():void
        {
            if (!rect) return;
            drawWidget();
            drawGuides();
            drawPanel();
            var resize:Boolean = canResize();
            MenuStyle.fit(controls, tr(resize ? "placement.controls" : "placement.controlsMove"), BUTTON_HEIGHT + 12);
        }

        private function drawWidget():void
        {
            var g:Graphics = widget.graphics; g.clear();
            var box:Object = clip(rect) || {x:0, y:0, width:0, height:0};
            var active:Boolean = Boolean(dragging);
            g.lineStyle(2, active ? MenuStyle.WHITE : MenuStyle.ACCENT);
            g.beginFill(MenuStyle.ACCENT, active ? 0.34 : 0.24);
            g.drawRect(box.x, box.y, Math.max(1, box.width), Math.max(1, box.height)); g.endFill(); g.lineStyle();
            // The point x and y describe, when the schema binds them.
            if (Placement.has(row, "x") || Placement.has(row, "y")) {
                var ox:Number = (rect.left + (Number(row.originX) || 0) * rect.width) * scale;
                var oy:Number = (rect.top + (Number(row.originY) || 0) * rect.height) * scale;
                g.lineStyle(3, MenuStyle.INK); g.moveTo(ox - 9, oy); g.lineTo(ox + 9, oy); g.moveTo(ox, oy - 9); g.lineTo(ox, oy + 9);
                g.lineStyle(1, MenuStyle.WHITE); g.moveTo(ox - 8, oy); g.lineTo(ox + 8, oy); g.moveTo(ox, oy - 8); g.lineTo(ox, oy + 8);
                g.lineStyle();
            }
            // Name and size inside the rectangle when they fit, otherwise just outside it.
            var size:String = Placement.number(rect.width) + " x " + Placement.number(rect.height);
            if (Placement.has(row, "scale")) size += "  (" + Placement.scaleText(row, rect) + ")";
            var labelWidth:Number = Math.max(box.width - 8, 160);
            widgetTitle.width = widgetSize.width = labelWidth;
            MenuStyle.fit(widgetTitle, String(row.title));
            MenuStyle.fit(widgetSize, size);
            var needed:Number = widgetTitle.height + widgetSize.height - 4;
            var inside:Boolean = box.height >= needed + 4 && box.width >= Math.max(widgetTitle.textWidth, widgetSize.textWidth) + 16;
            var center:Number = box.x + box.width / 2;
            widgetTitle.x = widgetSize.x = Math.max(0, Math.min(canvasWidth - labelWidth, center - labelWidth / 2));
            if (inside) widgetTitle.y = box.y + (box.height - needed) / 2 - 2;
            else widgetTitle.y = box.y >= needed + 8 ? box.y - needed - 6 : box.y + box.height + 4;
            widgetSize.y = widgetTitle.y + widgetTitle.height - 6;
            // Handles for the edges a drag can move: corners keep proportions; edges change one size.
            var corners:Boolean = Placement.proportional(row) ? canResize() : Placement.sizable(row, true) && Placement.sizable(row, false);
            for each (var id:String in HANDLES) {
                var handle:Sprite = handles[id];
                var corner:Boolean = id.length == 2;
                var horizontal:Boolean = id.indexOf("e") >= 0 || id.indexOf("w") >= 0, vertical:Boolean = id.indexOf("n") >= 0 || id.indexOf("s") >= 0;
                handle.visible = corner ? corners : !Placement.proportional(row) && (horizontal ? Placement.sizable(row, true) : Placement.sizable(row, false));
                handle.x = box.x + (id.indexOf("w") >= 0 ? 0 : id.indexOf("e") >= 0 ? box.width : box.width / 2);
                handle.y = box.y + (id.indexOf("n") >= 0 ? 0 : id.indexOf("s") >= 0 ? box.height : box.height / 2);
                handle.alpha = resizing || dragging == id ? 1 : 0.85;
            }
        }

        // Lines from each edge of the rectangle to the screen's edge, labelled with the margin.
        // Margins a save writes are accented.
        private function drawGuides():void
        {
            var g:Graphics = guides.graphics; g.clear();
            var box:Object = clip(rect);
            for each (var side:String in ["left", "top", "right", "bottom"]) TextField(guideLabels[side]).visible = false;
            if (!box) return;
            var measured:Object = Placement.measure(row, rect);
            var middleX:Number = box.x + box.width / 2, middleY:Number = box.y + box.height / 2;
            var strength:Number = dragging ? 1 : 0.8;
            guide(g, "left", 0, middleY, box.x, middleY, measured.left, strength);
            guide(g, "right", box.x + box.width, middleY, canvasWidth, middleY, measured.right, strength);
            guide(g, "top", middleX, 0, middleX, box.y, measured.top, strength);
            guide(g, "bottom", middleX, box.y + box.height, middleX, canvasHeight, measured.bottom, strength);
            // Snapped lines run the full screen while dragging.
            g.lineStyle(1, MenuStyle.ACCENT, 0.9);
            if (!isNaN(snapX)) { g.moveTo(snapX * scale, 0); g.lineTo(snapX * scale, canvasHeight); }
            if (!isNaN(snapY)) { g.moveTo(0, snapY * scale); g.lineTo(canvasWidth, snapY * scale); }
            g.lineStyle();
        }
        private function guide(g:Graphics, side:String, x1:Number, y1:Number, x2:Number, y2:Number, value:Number, strength:Number):void
        {
            var length:Number = Math.abs(x2 - x1) + Math.abs(y2 - y1);
            if (length < 2) return;
            var color:uint = Placement.has(row, side) ? MenuStyle.ACCENT : MenuStyle.MUTED;
            var horizontal:Boolean = y1 == y2;
            g.lineStyle(1, color, strength);
            g.moveTo(x1, y1); g.lineTo(x2, y2);
            // End ticks across the line.
            if (horizontal) { g.moveTo(x1, y1 - 5); g.lineTo(x1, y1 + 5); g.moveTo(x2, y2 - 5); g.lineTo(x2, y2 + 5); }
            else { g.moveTo(x1 - 5, y1); g.lineTo(x1 + 5, y1); g.moveTo(x2 - 5, y2); g.lineTo(x2 + 5, y2); }
            g.lineStyle();
            var label:TextField = guideLabels[side];
            MenuStyle.setText(label, Placement.number(value));
            label.textColor = color == MenuStyle.ACCENT ? MenuStyle.ACCENT : MenuStyle.WHITE;
            var labelWidth:Number = label.textWidth + 12, labelHeight:Number = MenuStyle.SMALL_SIZE + 6;
            // Only label a gap the number fits beside.
            if (length < (horizontal ? labelWidth + 8 : labelHeight + 8)) return;
            var cx:Number = (x1 + x2) / 2, cy:Number = (y1 + y2) / 2;
            var lx:Number = horizontal ? cx - labelWidth / 2 : cx + 8, ly:Number = horizontal ? cy - labelHeight - 4 : cy - labelHeight / 2;
            lx = Math.max(2, Math.min(canvasWidth - labelWidth - 2, lx)); ly = Math.max(2, Math.min(canvasHeight - labelHeight - 2, ly));
            g.beginFill(MenuStyle.INK, 0.85); g.drawRect(lx, ly, labelWidth, labelHeight); g.endFill();
            label.width = labelWidth; label.x = lx; label.y = ly - 3;
            label.visible = true;
        }

        // Rows below the mode readout depend on what this placement binds.
        private function layoutPanel():void
        {
            var y:Number = CONTENT_TOP;
            var resize:Boolean = canResize();
            modeLabel.visible = modeValue.visible = modeHint.visible = resize;
            if (resize) y += LINE_PITCH + MenuStyle.SMALL_SIZE + 22;
            shapeLabel.y = y; y += MenuStyle.SMALL_SIZE + 16;
            var showScale:Boolean = Placement.has(row, "scale") || Placement.proportional(row);
            for each (var role:String in READOUTS) {
                var readout:Object = readouts[role];
                var shown:Boolean = role != "scale" || showScale;
                readout.label.visible = readout.value.visible = shown;
                if (!shown) continue;
                readout.label.y = readout.value.y = y; y += LINE_PITCH;
            }
            y += 14;
            savesLabel.y = y; y += MenuStyle.SMALL_SIZE + 16;
            var count:int = 0;
            for each (role in Placement.ROLES) {
                if (!Placement.has(row, role) || count >= saves.length) continue;
                var line:Object = saves[count++];
                line.role = role;
                line.label.y = line.value.y = y; y += LINE_PITCH;
                MenuStyle.fit(line.label, String(row.bound[role].title));
            }
            for (var i:int = 0; i < saves.length; ++i) saves[i].label.visible = saves[i].value.visible = i < count;
            for (i = count; i < saves.length; ++i) saves[i].role = null;
            feedback.y = y + 10;
        }

        private function drawPanel():void
        {
            MenuStyle.setText(modeValue, tr(resizing ? "placement.modeResize" : "placement.modeMove"));
            MenuStyle.fit(modeHint, tr(resizing ? "placement.switchToMove" : "placement.switchToResize"));
            var measured:Object = Placement.measure(row, rect);
            for each (var role:String in READOUTS) {
                var readout:Object = readouts[role];
                if (!readout.value.visible) continue;
                MenuStyle.setText(readout.value, role == "scale" ? Placement.scaleText(row, rect) : Placement.number(measured[role]));
                readout.label.textColor = Placement.has(row, role) ? MenuStyle.WHITE : MenuStyle.MUTED;
            }
            var next:Object = Placement.values(row, rect), saved:Object = Placement.current(row);
            for each (var line:Object in saves) {
                if (!line.role) continue;
                var target:Object = row.bound[line.role];
                MenuStyle.setText(line.value, target.type == "int" ? String(next[line.role]) : NumericSetting.text(target, next[line.role]));
                line.value.textColor = next[line.role] != saved[line.role] ? MenuStyle.ACCENT : MenuStyle.WHITE;
            }
        }

        CONFIG::testHarness {
            public function testState():Object {
                return {active:visible, rect:rect ? copy(rect) : null, values:visible ? Placement.values(row, rect) : null,
                    resizing:resizing, dragging:dragging, feedback:feedback.text};
            }
        }
    }
}
