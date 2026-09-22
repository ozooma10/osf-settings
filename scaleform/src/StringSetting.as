package
{
    import flash.display.Sprite;
    import flash.events.Event;
    import flash.events.TextEvent;
    import flash.text.TextField;

    // A draft is published only after the native setter reports a saved value.
    public final class StringSetting extends Sprite
    {
        public var input:TextField;
        public var row:Object;
        private var feedback:TextField;

        public function StringSetting()
        {
            x = 1210; y = 754;
            graphics.lineStyle(1, MenuStyle.LINE); graphics.drawRect(0, 30, 634, 52);
            var label:TextField = MenuStyle.field(Localization.text("strings.edit"), 0, 0, 634, 30, 21, MenuStyle.MUTED, true);
            addChild(label);
            input = MenuStyle.field("", 8, 34, 618, 44, CONFIG::largeText ? 30 : 27);
            input.name = "stringValue"; input.type = "input";
            input.selectable = true; input.mouseEnabled = true;
            input.multiline = false; input.wordWrap = false;
            // maxChars counts UTF-16 units and truncates pasted text. Validate
            // explicitly instead so UTF-8 limits and failed drafts stay visible.
            input.addEventListener(Event.CHANGE, changed);
            input.addEventListener(TextEvent.TEXT_INPUT, entering);
            addChild(input);
            feedback = MenuStyle.field("", 0, 90, 634, 64, 21, MenuStyle.MUTED, true);
            feedback.multiline = true; feedback.wordWrap = true; addChild(feedback);
            visible = false;
        }

        public function open(value:Object):void
        {
            row = value; visible = true;
            MenuStyle.setText(input, String(row.value));
            stage.focus = input; input.setSelection(0, input.length); changed();
        }
        public function close():void { row = null; visible = false; }
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
            MenuStyle.setText(feedback, bytes < 0 ? Localization.text("strings.invalid") :
                Localization.text("strings.bytes", {bytes:bytes, limit:row.maxLength}));
        }
        private function entering(event:TextEvent):void
        {
            if (byteLength(event.text) < 0) {
                event.preventDefault(); showError(Localization.text("strings.invalid"));
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
