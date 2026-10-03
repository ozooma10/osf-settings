package
{
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.TextEvent;
    import flash.text.TextField;

    // A draft is published only after the native setter reports a saved value.
    public final class StringSetting extends Sprite
    {
        public var input:TextField;
        public var row:Object;
        private var feedback:TextField;
        private var bridge:Object;
        private var finished:Function;
        private var openedFrame:int;
        private var confirmHeld:Boolean;

        public function StringSetting(code:Object, onFinished:Function)
        {
            bridge = code; finished = onFinished;
            // The menu sets y under the selected setting's details.
            var column:Number = MenuStyle.DETAIL_WIDTH, box:Number = MenuStyle.DETAIL_BODY_SIZE + 22;
            x = MenuStyle.DETAIL_X;
            graphics.lineStyle(1, MenuStyle.LINE); graphics.drawRect(0, 26, column, box);
            var label:TextField = MenuStyle.field(tr("strings.edit"), 0, 0, column, 26, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            addChild(label);
            input = MenuStyle.field("", 8, 30, column - 16, box - 6, MenuStyle.DETAIL_BODY_SIZE);
            // The shared-font field is timeline-authored, so its name is read-only.
            input.type = "input";
            input.selectable = true; input.mouseEnabled = true;
            input.multiline = false; input.wordWrap = false;
            // maxChars counts UTF-16 units and truncates pasted text. Validate
            // explicitly instead so UTF-8 limits and failed drafts stay visible.
            input.addEventListener(Event.CHANGE, changed);
            input.addEventListener(TextEvent.TEXT_INPUT, entering);
            addChild(input);
            feedback = MenuStyle.field("", 0, 34 + box, column, 60, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            feedback.multiline = true; feedback.wordWrap = true; addChild(feedback);
            visible = false;
        }

        public function open(value:Object, frame:int):Boolean
        {
            if (!bridge.textInput(true)) return false;
            row = value; visible = true; openedFrame = frame; confirmHeld = false;
            MenuStyle.setText(input, String(row.value));
            stage.focus = input; input.setSelection(0, input.length); changed();
            return true;
        }
        public function close():void
        {
            if (!visible) return;
            row = null; visible = false; confirmHeld = false;
            bridge.textInput(false);
        }
        public function cancel():void { if (visible) finish(true); }
        private function finish(cancelled:Boolean):void
        {
            close(); finished(cancelled);
        }
        public function save(frame:int):void
        {
            if (!visible || frame <= openedFrame + 1) return;
            if (!valid) {
                showError(tr("strings.limit", {limit:row.maxLength}));
                stage.focus = input; return;
            }
            var value:String = input.text;
            var result:Object = bridge.setString(row.mod, row.key, value, Number(byteLength(value)));
            if (!result || !result.ok) {
                showError(result ? result.error : tr("errors.saveText"));
                stage.focus = input; return;
            }
            finish(false);
        }
        public function userEvent(name:String, pressed:Boolean, frame:int, event:KeyboardEvent = null):Boolean
        {
            if (!event) {
                if (name == "Cancel") { if (!pressed) cancel(); return true; }
                return false; // Let native forward raw keys and characters to the field.
            }
            if (name == "Accept" || name == "Cancel") {
                event.stopImmediatePropagation(); event.preventDefault();
                if (pressed) {
                    if (name == "Accept" && frame > openedFrame + 1) confirmHeld = true;
                } else {
                    if (frame > openedFrame + 1) {
                        if (name == "Accept") { if (confirmHeld) save(frame); }
                        else cancel();
                    }
                    confirmHeld = false;
                }
            }
            return true;
        }
        CONFIG::testHarness {
            public function testState():Object {
                return {active:visible, text:input.text, valid:valid, feedback:feedback.text,
                    focused:stage && stage.focus == input, maxLength:row ? row.maxLength : 0};
            }
        }
        public function get valid():Boolean
        {
            var bytes:int = byteLength(input.text);
            return row && bytes >= 0 && bytes <= int(row.maxLength);
        }
        public function showError(message:String):void
        {
            feedback.textColor = MenuStyle.ACCENT; MenuStyle.setText(feedback, message);
        }
        private function changed(event:Event = null):void
        {
            if (!row) return;
            var bytes:int = byteLength(input.text);
            feedback.textColor = valid ? MenuStyle.MUTED : MenuStyle.ACCENT;
            MenuStyle.setText(feedback, bytes < 0 ? tr("strings.invalid") :
                tr("strings.bytes", {bytes:bytes, limit:row.maxLength}));
        }
        private function entering(event:TextEvent):void
        {
            if (byteLength(event.text) < 0) {
                event.preventDefault(); showError(tr("strings.invalid"));
            }
        }
        // Match native IsValidString, decoding AS3 UTF-16 before counting UTF-8.
        public static function byteLength(text:String):int
        {
            var bytes:int = 0;
            for (var i:int = 0; i < text.length; ++i) {
                var code:uint = text.charCodeAt(i);
                if (code >= 0xD800 && code <= 0xDBFF) {
                    if (++i >= text.length) return -1;
                    var low:uint = text.charCodeAt(i);
                    if (low < 0xDC00 || low > 0xDFFF) return -1;
                    code = 0x10000 + ((code - 0xD800) << 10) + low - 0xDC00;
                } else if (code >= 0xDC00 && code <= 0xDFFF) return -1;
                if (code < 0x20 || code >= 0x7F && code <= 0x9F || code == 0x2028 || code == 0x2029) return -1;
                bytes += code < 0x80 ? 1 : code < 0x800 ? 2 : code < 0x10000 ? 3 : 4;
            }
            return bytes;
        }
    }
}
