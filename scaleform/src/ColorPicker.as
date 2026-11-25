package
{
    import flash.display.GradientType;
    import flash.display.Graphics;
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.FocusEvent;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Matrix;
    import flash.text.TextField;
    import flash.ui.Keyboard;

    // Modal color editor: a saturation/brightness field, hue and opacity bars, channel rows,
    // a hex field and basic colors. Nothing is saved until the native setter accepts the color.
    // Up/Down choose a row, Left/Right adjust it (held input accelerates), Accept saves, Back cancels.
    public final class ColorPicker extends Sprite
    {
        public var row:Object;
        private static const LARGE:Boolean = CONFIG::largeText;
        private static const BOX_WIDTH:Number = LARGE ? 1180 : 1080;
        private static const BOX_HEIGHT:Number = LARGE ? 800 : 720;
        private static const BOX_X:Number = (1920 - BOX_WIDTH) / 2;
        private static const BOX_Y:Number = (1080 - BOX_HEIGHT) / 2;
        private static const PAD:Number = 36;
        private static const FIELD:Number = LARGE ? 400 : 340;
        private static const BAR_WIDTH:Number = 30;
        private static const PREVIEW_WIDTH:Number = 120;
        private static const ROW_HEIGHT:Number = LARGE ? 46 : 38;
        private static const ROW_PITCH:Number = ROW_HEIGHT + 4;
        private static const LABEL_WIDTH:Number = 150;
        private static const CONTENT_TOP:Number = BOX_Y + (LARGE ? 128 : 118);
        private static const SWATCH_WIDTH:Number = LARGE ? 42 : 40;
        private static const SWATCH_HEIGHT:Number = LARGE ? 32 : 28;
        private static const SWATCH_GAP:Number = 8;
        private static const PALETTE_COLUMNS:int = 12;
        private static const BUTTON_WIDTH:Number = LARGE ? 210 : 190;
        private static const BUTTON_HEIGHT:Number = LARGE ? 56 : 50;
        // White and the primaries, their darker shades, then neutrals and the menu's own palette.
        private static const PALETTE:Array = [
            0xFFFFFF, 0xFF0000, 0xFF8000, 0xFFFF00, 0x80FF00, 0x00FF00, 0x00FF80, 0x00FFFF, 0x0080FF, 0x0000FF, 0x8000FF, 0xFF00FF,
            0x000000, 0x800000, 0x804000, 0x808000, 0x408000, 0x008000, 0x008040, 0x008080, 0x004080, 0x000080, 0x400080, 0x800080,
            0x404040, 0x808080, 0xC0C0C0, 0xF1F2EC, 0xD6B77D, 0xDE9D65, 0xC65C45, 0x719EB5, 0xAABCC6, 0x52646D, 0x24363F, 0x08151C];

        private var bridge:Object;
        private var finished:Function;
        private var heading:TextField;
        private var subtitle:TextField;
        private var field:Sprite = new Sprite();
        private var fieldCursor:Sprite = new Sprite();
        private var hueBar:Sprite = new Sprite();
        private var alphaBar:Sprite = new Sprite();
        private var preview:Sprite = new Sprite();
        private var newLabel:TextField;
        private var currentLabel:TextField;
        private var hexBox:Sprite = new Sprite();
        private var hexLabel:TextField;
        private var hexInput:TextField;
        private var paletteLabel:TextField;
        private var palette:Sprite = new Sprite();
        private var feedback:TextField;
        private var controls:TextField;
        private var buttons:Array = [];
        // Focusable rows in order: hex, the channels this setting edits, then the palette.
        private var rows:Array = [];
        private var channels:Object = {};
        private var focus:int = 1;
        private var paletteIndex:int = 0;
        private var allowAlpha:Boolean;
        private var original:Object;
        private var color:Object;
        private var hue:Number = 0;
        private var saturation:Number = 0;
        private var brightness:Number = 0;
        private var typing:Boolean;
        private var typingKey:uint;
        private var acceptBlocked:Boolean;
        private var acceptDown:Boolean;
        private var cancelDown:Boolean;
        private var holdDirection:int;
        private var holdFrames:int;
        private var dragging:String = "";
        private var mouseDownButton:Object;
        private var rowWidth:Number = 0;

        public function ColorPicker(code:Object, onFinished:Function)
        {
            bridge = code; finished = onFinished;
            graphics.beginFill(0, 0.85); graphics.drawRect(-4096, -4096, 10000, 10000); graphics.endFill();
            graphics.beginFill(MenuStyle.ROW); graphics.lineStyle(1, MenuStyle.LINE);
            graphics.drawRect(BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT); graphics.endFill(); graphics.lineStyle();
            MenuDecoration.corners(graphics, BOX_X + 10, BOX_Y + 10, BOX_WIDTH - 20, BOX_HEIGHT - 20);
            var left:Number = BOX_X + PAD, inner:Number = BOX_WIDTH - 2 * PAD;
            heading = text(tr("colors.title"), left, BOX_Y + 28, inner, MenuStyle.DETAIL_TITLE_SIZE + 20, MenuStyle.DETAIL_TITLE_SIZE + 3, MenuStyle.WHITE, true);
            subtitle = text("", left, BOX_Y + 34 + MenuStyle.DETAIL_TITLE_SIZE + 12, inner, MenuStyle.DETAIL_BODY_SIZE + 14, MenuStyle.DETAIL_BODY_SIZE, MenuStyle.MUTED);

            field.x = left; field.y = CONTENT_TOP; field.buttonMode = true; addChild(field);
            fieldCursor.mouseEnabled = false; field.addChild(fieldCursor);
            field.addEventListener(MouseEvent.MOUSE_DOWN, beginDrag);
            hueBar.y = alphaBar.y = CONTENT_TOP; hueBar.buttonMode = alphaBar.buttonMode = true;
            hueBar.name = "hueBar"; alphaBar.name = "alphaBar"; field.name = "field";
            addChild(hueBar); addChild(alphaBar);
            for each (var bar:Sprite in [hueBar, alphaBar]) {
                bar.addEventListener(MouseEvent.MOUSE_DOWN, beginDrag);
                bar.addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
            }
            preview.y = CONTENT_TOP + MenuStyle.SMALL_SIZE + 10; preview.mouseEnabled = false; addChild(preview);
            newLabel = text(tr("colors.new"), 0, CONTENT_TOP - 4, PREVIEW_WIDTH, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            currentLabel = text(tr("colors.current"), 0, 0, PREVIEW_WIDTH, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);

            // The hex field is a row of its own; Accept or a click starts typing into it.
            hexBox.name = "hex"; hexBox.buttonMode = true; addChild(hexBox);
            hexLabel = MenuStyle.field(tr("colors.hex"), 12, 0, LABEL_WIDTH - 16, ROW_HEIGHT, MenuStyle.VALUE_SIZE, MenuStyle.MUTED, true);
            hexBox.addChild(hexLabel);
            hexInput = MenuStyle.field("", 0, 0, 200, ROW_HEIGHT, MenuStyle.VALUE_SIZE, MenuStyle.WHITE);
            hexInput.multiline = false; hexInput.wordWrap = false;
            hexBox.addChild(hexInput);
            hexBox.addEventListener(MouseEvent.MOUSE_DOWN, hexPressed);
            rows.push({id:"hex", view:hexBox});
            for each (var spec:Array in [["hue", "colors.hue", 359], ["saturation", "colors.saturation", 100], ["brightness", "colors.brightness", 100],
                    ["red", "colors.red", 255], ["green", "colors.green", 255], ["blue", "colors.blue", 255], ["alpha", "colors.opacity", 255]]) {
                var channel:Object = {id:spec[0], maximum:spec[2], view:new Sprite(), track:new Sprite()};
                channel.view.name = channel.id; channel.view.buttonMode = true;
                channel.label = MenuStyle.field(tr(spec[1]), 12, 0, LABEL_WIDTH - 16, ROW_HEIGHT, MenuStyle.VALUE_SIZE, MenuStyle.MUTED, true);
                channel.value = MenuStyle.field("", 0, 0, 70, ROW_HEIGHT, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true);
                MenuStyle.align(channel.value, "right");
                channel.view.addChild(channel.track); channel.view.addChild(channel.label); channel.view.addChild(channel.value);
                channel.track.name = channel.id;
                channel.view.addEventListener(MouseEvent.MOUSE_DOWN, beginDrag);
                channel.view.addEventListener(MouseEvent.MOUSE_WHEEL, wheel);
                addChild(channel.view);
                channels[channel.id] = channel;
            }

            paletteLabel = text(tr("colors.basic"), left, 0, 500, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            palette.x = left; palette.buttonMode = true; addChild(palette);
            palette.addEventListener(MouseEvent.MOUSE_DOWN, palettePressed);

            feedback = text("", 0, 0, 0, MenuStyle.SMALL_SIZE * 2 + 18, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            feedback.multiline = feedback.wordWrap = true;
            controls = text("", left, BOX_Y + BOX_HEIGHT - 46, inner, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE, MenuStyle.MUTED);
            for (var i:int = 0; i < 2; ++i) {
                var button:Sprite = new Sprite(); button.name = i == 0 ? "cancel" : "save";
                button.buttonMode = true; button.mouseChildren = false;
                button.x = BOX_X + BOX_WIDTH - PAD - (2 - i) * BUTTON_WIDTH - (1 - i) * 20;
                button.y = BOX_Y + BOX_HEIGHT - 70 - BUTTON_HEIGHT;
                var caption:TextField = MenuStyle.field(i == 0 ? tr("buttons.cancel") : tr("buttons.save"), 0, (BUTTON_HEIGHT - MenuStyle.DETAIL_BODY_SIZE) / 2 - 4,
                    BUTTON_WIDTH, MenuStyle.DETAIL_BODY_SIZE + 14, MenuStyle.DETAIL_BODY_SIZE, i == 0 ? MenuStyle.WHITE : MenuStyle.INK, true);
                MenuStyle.align(caption, "center");
                button.addChild(caption);
                button.graphics.beginFill(i == 0 ? MenuStyle.INK : MenuStyle.WHITE); button.graphics.lineStyle(1, MenuStyle.LINE);
                button.graphics.drawRect(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT); button.graphics.endFill();
                button.addEventListener(MouseEvent.MOUSE_DOWN, buttonPressed);
                button.addEventListener(MouseEvent.CLICK, buttonClicked);
                buttons.push(button); addChild(button);
            }
            addEventListener(MouseEvent.MOUSE_DOWN, pressedAnywhere, true);
            visible = false;
        }

        private function text(value:String, x:Number, y:Number, width:Number, height:Number, size:Number, color:uint, label:Boolean = false):TextField
        {
            var result:TextField = MenuStyle.field(value, x, y, width, height, size, color, label);
            addChild(result); return result;
        }

        // held: Accept was already down when the picker opened, so its release must not save.
        public function open(value:Object, held:Boolean):void
        {
            row = value;
            original = ColorSetting.parse(String(value.value)) || {r:255, g:255, b:255, a:255};
            allowAlpha = Boolean(value.alpha);
            if (!allowAlpha) original.a = 255;
            setColor(original, true);
            acceptBlocked = held; acceptDown = cancelDown = false; holdDirection = 0; typing = false; dragging = ""; mouseDownButton = null;
            paletteIndex = Math.max(0, PALETTE.indexOf(ColorSetting.rgb(color)));
            rows.length = 1;
            for each (var id:String in ["hue", "saturation", "brightness", "red", "green", "blue"]) rows.push(channels[id]);
            if (allowAlpha) rows.push(channels.alpha);
            rows.push({id:"palette", view:palette});
            channels.alpha.view.visible = alphaBar.visible = allowAlpha;
            focus = 1;
            MenuStyle.fit(subtitle, String(value.title));
            MenuStyle.setText(feedback, "");
            layout();
            visible = true;
            if (stage) {
                stage.addEventListener(FocusEvent.KEY_FOCUS_CHANGE, keepFocus, true);
                stage.focus = null;
            }
            addEventListener(Event.ENTER_FRAME, advance);
            draw();
        }

        public function cancel():void { if (visible) finish(false); }

        // Leaves without reporting, for menu teardown.
        public function close():void
        {
            if (!visible) return;
            stopTyping();
            endDrag();
            removeEventListener(Event.ENTER_FRAME, advance);
            if (stage) stage.removeEventListener(FocusEvent.KEY_FOCUS_CHANGE, keepFocus, true);
            visible = false; row = null; holdDirection = 0; acceptDown = cancelDown = false; mouseDownButton = null;
        }

        private function finish(saved:Boolean):void
        {
            close(); finished(saved);
        }

        private function save():void
        {
            if (!visible || typing) return;
            var value:String = ColorSetting.format(color);
            if (value == ColorSetting.format(original)) { finish(false); return; }
            var result:Object = bridge.setColor(row.mod, row.key, value);
            if (!result || !result.ok) { showError(result && result.error ? String(result.error) : tr("errors.save")); return; }
            finish(true);
        }

        // Named game events and raw keys share this path; raw keys arrive only when native declines a named event.
        public function userEvent(name:String, pressed:Boolean, event:KeyboardEvent = null):Boolean
        {
            if (!visible) return false;
            if (typing) {
                if (!event) {
                    // Text input context: native forwards characters and editing keys to the field.
                    if (name == "Cancel") { if (!pressed) finishTyping(false); return true; }
                    return false;
                }
                if (event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.ESCAPE) {
                    event.stopImmediatePropagation(); event.preventDefault();
                    if (pressed) typingKey = event.keyCode;
                    else if (typingKey == event.keyCode) { typingKey = 0; finishTyping(event.keyCode == Keyboard.ENTER); }
                }
                return true;
            }
            if (event) { event.stopImmediatePropagation(); if (pressed) event.preventDefault(); }
            if (name == "Accept") {
                if (pressed) { if (!acceptBlocked) acceptDown = true; }
                else {
                    var act:Boolean = !acceptBlocked && acceptDown;
                    acceptBlocked = acceptDown = false;
                    if (act) { if (rows[focus].id == "hex") startTyping(); else save(); }
                }
            } else if (name == "Cancel") {
                if (pressed) cancelDown = true;
                else if (cancelDown) { cancelDown = false; cancel(); }
            } else if (name == "Up" || name == "Down") {
                if (pressed) move(name == "Up" ? -1 : 1);
            } else if (name == "Left" || name == "Right") {
                var direction:int = name == "Left" ? -1 : 1;
                if (pressed) {
                    // Keyboard auto-repeat sends more presses; the frame timer owns repetition.
                    if (holdDirection != direction) { holdDirection = direction; holdFrames = 0; step(direction, 1); }
                } else if (holdDirection == direction) holdDirection = 0;
            } else if (name == "LShoulder" || name == "RShoulder") {
                if (pressed) step(name == "LShoulder" ? -1 : 1, 10);
            }
            return true;
        }

        private function advance(event:Event):void
        {
            if (!holdDirection || typing) return;
            ++holdFrames;
            var id:String = rows[focus].id;
            // About a third of a second before repeating, then faster the longer it is held.
            if (id == "palette") { if (holdFrames > 20 && holdFrames % 6 == 0) step(holdDirection, 1); }
            else if (holdFrames > 20 && holdFrames % 2 == 0) step(holdDirection, holdFrames > 120 ? 8 : holdFrames > 60 ? 3 : 1);
        }

        private function move(direction:int):void
        {
            var id:String = rows[focus].id;
            // The palette is a grid: Up/Down walk its rows before leaving it.
            if (id == "palette") {
                var next:int = paletteIndex + direction * PALETTE_COLUMNS;
                if (next >= 0 && next < PALETTE.length) { paletteIndex = next; applyPalette(); return; }
                if (direction > 0) return;
            }
            var target:int = Math.max(0, Math.min(rows.length - 1, focus + direction));
            if (target == focus) return;
            focus = target; holdDirection = 0;
            if (rows[focus].id == "palette") paletteIndex = paletteIndex % PALETTE_COLUMNS;
            draw();
        }

        private function step(direction:int, size:int):void
        {
            var id:String = rows[focus].id;
            if (id == "hex") return;
            if (id == "palette") {
                paletteIndex = (paletteIndex + direction + PALETTE.length) % PALETTE.length;
                applyPalette(); return;
            }
            setChannel(id, channelValue(id) + direction * size);
        }

        private function channelValue(id:String):Number
        {
            switch (id) {
                case "hue": return Math.round(hue) % 360;
                case "saturation": return Math.round(saturation * 100);
                case "brightness": return Math.round(brightness * 100);
                case "red": return color.r;
                case "green": return color.g;
                case "blue": return color.b;
            }
            return color.a;
        }

        private function setChannel(id:String, value:Number):void
        {
            var maximum:Number = channels[id].maximum;
            if (id == "hue") value = ((Math.round(value) % 360) + 360) % 360;
            else value = Math.max(0, Math.min(maximum, Math.round(value)));
            if (id == "hue") setHsv(value, saturation, brightness);
            else if (id == "saturation") setHsv(hue, value / 100, brightness);
            else if (id == "brightness") setHsv(hue, saturation, value / 100);
            else if (id == "alpha") { color.a = allowAlpha ? value : 255; draw(); }
            else {
                var next:Object = {r:color.r, g:color.g, b:color.b, a:color.a};
                next[id == "red" ? "r" : id == "green" ? "g" : "b"] = value;
                setColor(next, false);
            }
        }

        private function setHsv(h:Number, s:Number, v:Number):void
        {
            hue = h; saturation = Math.max(0, Math.min(1, s)); brightness = Math.max(0, Math.min(1, v));
            color = ColorSetting.fromHsv(hue, saturation, brightness, color.a);
            draw();
        }

        // Channel edits keep the hue (and saturation at black) the channels cannot express.
        private function setColor(value:Object, reset:Boolean):void
        {
            color = {r:value.r, g:value.g, b:value.b, a:allowAlpha ? value.a : 255};
            var hsv:Object = ColorSetting.toHsv(color);
            if (reset || hsv.s > 0) hue = hsv.h;
            if (reset || hsv.v > 0) saturation = hsv.s;
            brightness = hsv.v;
            if (!reset) draw();
        }

        private function applyPalette():void
        {
            var rgb:uint = PALETTE[paletteIndex];
            setColor({r:rgb >> 16 & 0xFF, g:rgb >> 8 & 0xFF, b:rgb & 0xFF, a:color.a}, false);
        }

        // Hex text: "#RRGGBB", "RRGGBB", and with opacity "#RRGGBBAA" or "RRGGBBAA".
        private function startTyping():void
        {
            if (typing) return;
            typing = true; typingKey = 0; holdDirection = 0;
            focus = 0;
            bridge.textInput(true);
            hexInput.type = "input"; hexInput.selectable = true; hexInput.mouseEnabled = true;
            hexInput.restrict = "#0-9A-Fa-f"; hexInput.maxChars = 9;
            MenuStyle.setText(hexInput, ColorSetting.display(color, allowAlpha));
            if (stage) { stage.focus = hexInput; hexInput.setSelection(0, hexInput.length); }
            feedback.textColor = MenuStyle.MUTED;
            MenuStyle.setText(feedback, tr(allowAlpha ? "colors.typingAlpha" : "colors.typing"));
            draw();
        }

        // Returns false, still typing, when apply finds the text invalid.
        private function finishTyping(apply:Boolean):Boolean
        {
            if (!typing) return true;
            if (apply) {
                var value:String = hexInput.text;
                if (value.length == 6 || value.length == 8) value = "#" + value;
                var parsed:Object = ColorSetting.parse(value);
                if (!parsed || !allowAlpha && parsed.a != 255) {
                    showError(tr(allowAlpha ? "colors.invalidHexAlpha" : "colors.invalidHex"));
                    if (stage) stage.focus = hexInput;
                    return false;
                }
                if (value.length == 7) parsed.a = color.a;
                stopTyping();
                setColor(parsed, false);
            } else {
                stopTyping();
                draw();
            }
            MenuStyle.setText(feedback, "");
            return true;
        }

        private function stopTyping():void
        {
            if (!typing) return;
            typing = false; typingKey = 0;
            hexInput.type = "dynamic"; hexInput.selectable = false; hexInput.mouseEnabled = false;
            if (stage && stage.focus == hexInput) stage.focus = null;
            bridge.textInput(false);
        }

        private function showError(message:String):void
        {
            feedback.textColor = MenuStyle.ACCENT; MenuStyle.setText(feedback, message);
        }

        // Tab must not move focus out of the modal.
        private function keepFocus(event:FocusEvent):void { if (visible) event.preventDefault(); }

        private function pressedAnywhere(event:MouseEvent):void
        {
            // Clicking away from the hex field applies valid text, otherwise restores it.
            if (typing && event.target != hexInput && !finishTyping(true)) finishTyping(false);
        }

        private function hexPressed(event:MouseEvent):void
        {
            event.stopPropagation();
            if (!typing) { holdDirection = 0; startTyping(); }
        }

        private function palettePressed(event:MouseEvent):void
        {
            event.stopPropagation();
            var column:int = int(palette.mouseX / (SWATCH_WIDTH + SWATCH_GAP));
            var line:int = int(palette.mouseY / (SWATCH_HEIGHT + SWATCH_GAP));
            var index:int = line * PALETTE_COLUMNS + column;
            if (column < 0 || column >= PALETTE_COLUMNS || index < 0 || index >= PALETTE.length) return;
            focus = rows.length - 1; paletteIndex = index; applyPalette();
        }

        private function buttonPressed(event:MouseEvent):void { mouseDownButton = event.currentTarget; event.stopPropagation(); }
        private function buttonClicked(event:MouseEvent):void
        {
            event.stopPropagation();
            if (mouseDownButton != event.currentTarget) return;
            mouseDownButton = null;
            if (Sprite(event.currentTarget).name == "save") save(); else cancel();
        }

        // Bars and the field edit a channel row too; their names map to it.
        private static function channelOf(name:String):String
        {
            return name == "hueBar" ? "hue" : name == "alphaBar" ? "alpha" : name == "field" ? "saturation" : name;
        }
        private function beginDrag(event:MouseEvent):void
        {
            event.stopPropagation();
            var name:String = Sprite(event.currentTarget).name;
            var index:int = rowIndex(channelOf(name));
            if (index >= 0) focus = index;
            holdDirection = 0;
            // A channel row's label only selects it; its track takes the value.
            var channel:Object = channels[name];
            if (channel && (channel.view.mouseX < channel.track.x - 10 || channel.view.mouseX > channel.track.x + trackWidth() + 10)) { draw(); return; }
            dragging = name;
            if (stage) {
                stage.addEventListener(MouseEvent.MOUSE_MOVE, dragMoved);
                stage.addEventListener(MouseEvent.MOUSE_UP, endDrag);
            }
            dragMoved();
        }
        private function dragMoved(event:MouseEvent = null):void
        {
            if (!dragging) return;
            if (dragging == "field") {
                setHsv(hue, clamp(field.mouseX / FIELD), 1 - clamp(field.mouseY / FIELD));
            } else if (dragging == "hueBar") {
                setHsv(Math.min(359, clamp(hueBar.mouseY / FIELD) * 360), saturation, brightness);
            } else if (dragging == "alphaBar") {
                setChannel("alpha", (1 - clamp(alphaBar.mouseY / FIELD)) * 255);
            } else if (channels[dragging]) {
                var channel:Object = channels[dragging];
                setChannel(dragging, clamp(Sprite(channel.track).mouseX / trackWidth()) * channel.maximum);
            }
        }
        private function endDrag(event:MouseEvent = null):void
        {
            dragging = "";
            if (stage) {
                stage.removeEventListener(MouseEvent.MOUSE_MOVE, dragMoved);
                stage.removeEventListener(MouseEvent.MOUSE_UP, endDrag);
            }
        }
        private function wheel(event:MouseEvent):void
        {
            event.stopPropagation();
            var name:String = Sprite(event.currentTarget).name;
            var id:String = channelOf(name);
            var index:int = rowIndex(id);
            if (index < 0 || !event.delta) return;
            focus = index;
            // The hue bar runs from 0 at the top; everything else grows up or to the right.
            var direction:int = event.delta > 0 ? 1 : -1;
            if (name == "hueBar") direction = -direction;
            setChannel(id, channelValue(id) + direction);
        }
        private function rowIndex(id:String):int
        {
            for (var i:int = 0; i < rows.length; ++i) if (rows[i].id == id) return i;
            return -1;
        }
        private static function clamp(value:Number):Number { return Math.max(0, Math.min(1, value)); }

        // Column positions depend on whether the opacity bar is shown.
        private function layout():void
        {
            var x:Number = BOX_X + PAD + FIELD + 20;
            hueBar.x = x; x += BAR_WIDTH + 16;
            if (allowAlpha) { alphaBar.x = x; x += BAR_WIDTH + 16; }
            x += 8;
            preview.x = newLabel.x = currentLabel.x = x;
            x += PREVIEW_WIDTH + PAD;
            var width:Number = BOX_X + BOX_WIDTH - PAD - x;
            for (var i:int = 0; i < rows.length - 1; ++i) {
                var view:Sprite = rows[i].view;
                view.x = x; view.y = CONTENT_TOP + i * ROW_PITCH;
                var channel:Object = channels[rows[i].id];
                if (channel) {
                    channel.track.x = LABEL_WIDTH; channel.track.y = (ROW_HEIGHT - 12) / 2;
                    channel.value.x = width - 82; channel.value.y = (ROW_HEIGHT - MenuStyle.VALUE_SIZE) / 2 - 4;
                    channel.label.y = channel.value.y;
                }
            }
            hexLabel.y = hexInput.y = (ROW_HEIGHT - MenuStyle.VALUE_SIZE) / 2 - 4;
            hexInput.x = LABEL_WIDTH + 6; hexInput.width = width - LABEL_WIDTH - 20;
            hexBox.graphics.clear();
            rowWidth = width;
            var previewHeight:Number = (FIELD - 2 * (MenuStyle.SMALL_SIZE + 10)) / 2;
            currentLabel.y = preview.y + previewHeight + 6;
            var paletteTop:Number = CONTENT_TOP + FIELD + 22;
            paletteLabel.y = paletteTop; palette.y = paletteTop + MenuStyle.SMALL_SIZE + 14;
            feedback.x = x; feedback.y = paletteTop; feedback.width = width;
        }
        private function trackWidth():Number { return Math.max(40, rowWidth - LABEL_WIDTH - 96); }

        private function draw():void
        {
            if (!color) return;
            drawField();
            drawBars();
            drawPreview();
            drawRows();
            drawPalette();
            buttons[1].alpha = typing ? 0.5 : 1;
            var acceptHint:String = typing ? tr("colors.controlsTyping") : rows[focus].id == "hex" ? tr("colors.controlsHex") : tr("colors.controls");
            MenuStyle.fit(controls, acceptHint);
        }

        private function drawField():void
        {
            var g:Graphics = field.graphics; g.clear();
            var pure:Object = ColorSetting.fromHsv(hue, 1, 1);
            g.beginFill(ColorSetting.rgb(pure)); g.drawRect(0, 0, FIELD, FIELD); g.endFill();
            gradient(g, 0, 0, FIELD, FIELD, [0xFFFFFF, 0xFFFFFF], [1, 0], false);
            gradient(g, 0, 0, FIELD, FIELD, [0x000000, 0x000000], [0, 1], true);
            g.lineStyle(1, focusedOn("saturation") || focusedOn("brightness") ? MenuStyle.WHITE : MenuStyle.LINE);
            g.drawRect(0, 0, FIELD, FIELD); g.lineStyle();
            var c:Graphics = fieldCursor.graphics; c.clear();
            var cx:Number = saturation * FIELD, cy:Number = (1 - brightness) * FIELD;
            c.lineStyle(3, MenuStyle.INK); c.drawCircle(cx, cy, 8);
            c.lineStyle(2, MenuStyle.WHITE); c.drawCircle(cx, cy, 8); c.lineStyle();
        }

        private function drawBars():void
        {
            var g:Graphics = hueBar.graphics; g.clear();
            var stops:Array = [];
            for (var i:int = 0; i <= 6; ++i) stops.push(ColorSetting.rgb(ColorSetting.fromHsv(i * 60 % 360, 1, 1)));
            gradient(g, 0, 0, BAR_WIDTH, FIELD, stops, [1, 1, 1, 1, 1, 1, 1], true);
            frame(g, BAR_WIDTH, FIELD, focusedOn("hue"));
            marker(g, hue / 360 * FIELD, BAR_WIDTH);
            if (!allowAlpha) return;
            g = alphaBar.graphics; g.clear();
            ColorSetting.checkerboard(g, 0, 0, BAR_WIDTH, FIELD, 8);
            var rgb:uint = ColorSetting.rgb(color);
            gradient(g, 0, 0, BAR_WIDTH, FIELD, [rgb, rgb], [1, 0], true);
            frame(g, BAR_WIDTH, FIELD, focusedOn("alpha"));
            marker(g, (1 - color.a / 255) * FIELD, BAR_WIDTH);
        }

        private function drawPreview():void
        {
            var g:Graphics = preview.graphics; g.clear();
            var height:Number = (FIELD - 2 * (MenuStyle.SMALL_SIZE + 10)) / 2;
            ColorSetting.drawSwatch(g, 0, 0, PREVIEW_WIDTH, height, color, MenuStyle.LINE);
            ColorSetting.drawSwatch(g, 0, height + MenuStyle.SMALL_SIZE + 22, PREVIEW_WIDTH, height, original, MenuStyle.LINE);
        }

        private function drawRows():void
        {
            var width:Number = rowWidth;
            for (var i:int = 0; i < rows.length - 1; ++i) {
                var item:Object = rows[i];
                var view:Sprite = item.view;
                var selected:Boolean = i == focus;
                var g:Graphics = view.graphics; g.clear();
                g.beginFill(selected ? MenuStyle.HOVER : MenuStyle.ROW, selected ? 1 : 0);
                g.lineStyle(1, selected ? MenuStyle.WHITE : MenuStyle.LINE, selected ? 1 : 0);
                g.drawRect(0, 0, width, ROW_HEIGHT); g.endFill(); g.lineStyle();
                if (item.id == "hex") {
                    hexLabel.textColor = selected ? MenuStyle.WHITE : MenuStyle.MUTED;
                    g.lineStyle(1, typing ? MenuStyle.ACCENT : MenuStyle.LINE);
                    g.beginFill(MenuStyle.INK); g.drawRect(hexInput.x - 6, 5, hexInput.width + 12, ROW_HEIGHT - 10); g.endFill(); g.lineStyle();
                    if (!typing) MenuStyle.setText(hexInput, ColorSetting.display(color, allowAlpha));
                    continue;
                }
                var channel:Object = item;
                channel.label.textColor = selected ? MenuStyle.WHITE : MenuStyle.MUTED;
                var value:Number = channelValue(channel.id);
                MenuStyle.setText(channel.value, channel.id == "saturation" || channel.id == "brightness" ? value + "%" : String(value));
                var t:Graphics = channel.track.graphics; t.clear();
                var length:Number = trackWidth();
                var stops:Array = trackColors(channel.id);
                if (channel.id == "alpha") ColorSetting.checkerboard(t, 0, 0, length, 12, 6);
                var alphas:Array = [];
                for (var s:int = 0; s < stops.length; ++s) alphas.push(channel.id == "alpha" ? s / (stops.length - 1) : 1);
                gradient(t, 0, 0, length, 12, stops, alphas, false);
                t.lineStyle(1, MenuStyle.LINE); t.drawRect(0, 0, length, 12); t.lineStyle();
                var position:Number = value / channel.maximum * length;
                t.lineStyle(3, MenuStyle.INK); t.moveTo(position, -5); t.lineTo(position, 17);
                t.lineStyle(1, MenuStyle.WHITE); t.moveTo(position, -5); t.lineTo(position, 17); t.lineStyle();
            }
        }

        // Each track shows what moving it would do, given the other channels.
        private function trackColors(id:String):Array
        {
            switch (id) {
                case "hue":
                    var hues:Array = [];
                    for (var i:int = 0; i <= 6; ++i) hues.push(ColorSetting.rgb(ColorSetting.fromHsv(i * 60 % 360, 1, 1)));
                    return hues;
                case "saturation": return [ColorSetting.rgb(ColorSetting.fromHsv(hue, 0, brightness)), ColorSetting.rgb(ColorSetting.fromHsv(hue, 1, brightness))];
                case "brightness": return [0x000000, ColorSetting.rgb(ColorSetting.fromHsv(hue, saturation, 1))];
                case "red": return [ColorSetting.rgb({r:0, g:color.g, b:color.b}), ColorSetting.rgb({r:255, g:color.g, b:color.b})];
                case "green": return [ColorSetting.rgb({r:color.r, g:0, b:color.b}), ColorSetting.rgb({r:color.r, g:255, b:color.b})];
                case "blue": return [ColorSetting.rgb({r:color.r, g:color.g, b:0}), ColorSetting.rgb({r:color.r, g:color.g, b:255})];
            }
            var rgb:uint = ColorSetting.rgb(color);
            return [rgb, rgb];
        }

        private function drawPalette():void
        {
            var g:Graphics = palette.graphics; g.clear();
            var focused:Boolean = rows[focus].id == "palette";
            var current:uint = ColorSetting.rgb(color);
            paletteLabel.textColor = focused ? MenuStyle.WHITE : MenuStyle.MUTED;
            for (var i:int = 0; i < PALETTE.length; ++i) {
                var x:Number = (i % PALETTE_COLUMNS) * (SWATCH_WIDTH + SWATCH_GAP);
                var y:Number = int(i / PALETTE_COLUMNS) * (SWATCH_HEIGHT + SWATCH_GAP);
                g.beginFill(PALETTE[i]); g.lineStyle(1, MenuStyle.LINE); g.drawRect(x, y, SWATCH_WIDTH, SWATCH_HEIGHT); g.endFill(); g.lineStyle();
                var cursor:Boolean = focused && i == paletteIndex;
                if (cursor || PALETTE[i] == current) {
                    g.lineStyle(cursor ? 3 : 1, MenuStyle.WHITE);
                    g.drawRect(x - 3, y - 3, SWATCH_WIDTH + 6, SWATCH_HEIGHT + 6); g.lineStyle();
                }
            }
        }

        private function focusedOn(id:String):Boolean { return rows[focus] && rows[focus].id == id; }

        private static function gradient(g:Graphics, x:Number, y:Number, width:Number, height:Number, colors:Array, alphas:Array, vertical:Boolean):void
        {
            var box:Matrix = new Matrix();
            box.createGradientBox(width, height, vertical ? Math.PI / 2 : 0, x, y);
            var ratios:Array = [];
            for (var i:int = 0; i < colors.length; ++i) ratios.push(Math.round(i * 255 / (colors.length - 1)));
            g.lineStyle();
            g.beginGradientFill(GradientType.LINEAR, colors, alphas, ratios, box);
            g.drawRect(x, y, width, height); g.endFill();
        }
        private static function frame(g:Graphics, width:Number, height:Number, focused:Boolean):void
        {
            g.lineStyle(1, focused ? MenuStyle.WHITE : MenuStyle.LINE); g.drawRect(0, 0, width, height); g.lineStyle();
        }
        private static function marker(g:Graphics, y:Number, width:Number):void
        {
            g.lineStyle(3, MenuStyle.INK); g.drawRect(-3, y - 3, width + 6, 6);
            g.lineStyle(1, MenuStyle.WHITE); g.drawRect(-3, y - 3, width + 6, 6); g.lineStyle();
        }

        CONFIG::testHarness {
            public function testState():Object {
                return {active:visible, color:color ? ColorSetting.format(color) : "", focus:visible ? rows[focus].id : "",
                    typing:typing, text:hexInput.text, feedback:feedback.text};
            }
        }
    }
}
