package
{
    import flash.display.BitmapData;
    import flash.display.DisplayObject;
    import flash.display.DisplayObjectContainer;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Matrix;
    import flash.text.Font;
    import flash.text.TextField;
    import flash.ui.Keyboard;
    import flash.utils.ByteArray;

    // Opt-in checks against the real movie and vanilla list, using -Pverify=true
    // with the design fixture. No operating-system input or game files are used.
    public final class PreviewChecks
    {
        private var menu:MovieClip;
        private var list:Object;
        private var input:TextField;
        private var tick:int = 0;
        private var step:int = 0;
        private var setter:Function;

        public function PreviewChecks(movie:MovieClip)
        {
            menu = movie;
            for (var i:int = 0; i < menu.numChildren; ++i) {
                var child:Object = menu.getChildAt(i);
                if ("selectedIndex" in child && "InitializeEntries" in child) list = child;
            }
            input = findInput(menu);
            menu.addEventListener(Event.ENTER_FRAME, advance);
        }
        private function require(value:Boolean, message:String):void
        {
            if (!value) throw new Error(message);
            trace("[verify] OK " + message);
        }
        private function userEvent(name:String):void
        {
            Object(menu).ProcessUserEvent(name, true); Object(menu).ProcessUserEvent(name, false);
        }
        private function key(code:uint, target:DisplayObject = null):void
        {
            (target || list).dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_DOWN, true, true, 0, code));
        }
        private function search(text:String):void
        {
            menu.stage.focus = input; input.text = text; input.dispatchEvent(new Event(Event.CHANGE));
        }
        private function advance(event:Event):void
        {
            if (++tick % 12) return;
            try {
                switch (step++) {
                case 0:
                    require(Object(menu).startupPhase == "ready" && list != null, "menu ready");
                    requireGlyphs("SELECTED SETTING");
                    requireGlyphs("Auto-advance stages");
                    require(list.entryCount == 6, "playback has six settings");
                    require(list.selectedEntry.row.key == "autoAdvance", "first setting selected");
                    capture("default"); userEvent("Accept"); break;
                case 1:
                    require(list.selectedEntry.row.value == false, "toggle saved and reread");
                    capture("changed"); key(Keyboard.B); break;
                case 2:
                    require(list.selectedEntry.row.value == true, "reset restored schema default");
                    findNamed(menu, "camera").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 3:
                    require(list.entryCount == 2 && list.selectedEntry.row.key == "freeCamera", "camera tab filters settings");
                    key(221); break;
                case 4:
                    require(list.entryCount == 1 && list.selectedEntry.row.key == "hotkeys", "keyboard changes group");
                    search("Diagnostic logging"); break;
                case 5:
                    require(list.entryCount == 1 && list.selectedEntry.row.key == "debug", "search spans this mod's groups");
                    search("no such preview setting"); break;
                case 6:
                    require(list.entryCount == 0 && list.selectedIndex == -1, "empty search clears selection");
                    key(Keyboard.ESCAPE, input); break;
                case 7:
                    require(list.entryCount == 1 && list.selectedEntry.row.key == "hotkeys", "cancel search restores page");
                    userEvent("Cancel"); break;
                case 8:
                    require(list.entryCount == 1 && list.selectedEntry.row.title == "OSF Director", "back opens mod list");
                    userEvent("Accept"); break;
                case 9:
                    require(list.entryCount == 6, "opening a mod returns to playback");
                    list.selectedIndex = 5; break;
                case 10:
                    require(list.selectedEntry.row.key == "restoreView", "last row reachable");
                    capture("last-row");
                    setter = Object(menu).BGSCodeObj.setBool;
                    Object(menu).BGSCodeObj.setBool = function(mod:String, key:String, value:Boolean):Object {
                        return {ok:false, error:"Could not save this setting. Your previous value is unchanged."};
                    };
                    userEvent("Accept"); break;
                case 11:
                    require(list.selectedEntry.row.value == true, "save failure preserves previous value");
                    Object(menu).BGSCodeObj.setBool = setter;
                    menu.removeEventListener(Event.ENTER_FRAME, advance);
                    trace("[verify] PASS"); break;
                }
            } catch (error:Error) {
                menu.removeEventListener(Event.ENTER_FRAME, advance); trace("[verify] FAIL " + error);
            }
        }
        private function requireGlyphs(text:String):void
        {
            var field:TextField;
            for (var i:int = 0; i < menu.numChildren; ++i) {
                var candidate:TextField = menu.getChildAt(i) as TextField;
                if (candidate && candidate.text == text) { field = candidate; break; }
            }
            require(field != null, "custom text present: " + text);
            var embedded:Boolean = false;
            for each (var font:Font in Font.enumerateFonts(false)) {
                if (font.fontName == field.defaultTextFormat.font) embedded = true;
            }
            require(embedded, "custom text uses an embedded game font: " + text);
            var first:BitmapData = new BitmapData(96, 96, true, 0);
            var second:BitmapData = new BitmapData(96, 96, true, 0);
            field.text = "M"; first.draw(field);
            field.text = "I"; second.draw(field);
            field.text = text;
            var visible:Boolean = !first.getColorBoundsRect(0xFF000000, 0, false).isEmpty() &&
                                  !second.getColorBoundsRect(0xFF000000, 0, false).isEmpty();
            var difference:Object = first.compare(second);
            var distinct:Boolean = difference is BitmapData;
            if (distinct) BitmapData(difference).dispose();
            first.dispose(); second.dispose();
            require(visible && distinct, "custom font renders distinct glyphs: " + text);
        }
        private function findInput(container:DisplayObjectContainer):TextField
        {
            for (var i:int = 0; i < container.numChildren; ++i) {
                var child:DisplayObject = container.getChildAt(i);
                if (child is TextField && TextField(child).type == "input") return child as TextField;
                if (child is DisplayObjectContainer) {
                    var result:TextField = findInput(child as DisplayObjectContainer); if (result) return result;
                }
            }
            return null;
        }
        private function findNamed(container:DisplayObjectContainer, name:String):DisplayObject
        {
            for (var i:int = 0; i < container.numChildren; ++i) {
                var child:DisplayObject = container.getChildAt(i);
                if (child.name == name) return child;
                if (child is DisplayObjectContainer) {
                    var result:DisplayObject = findNamed(child as DisplayObjectContainer, name); if (result) return result;
                }
            }
            return null;
        }
        private function capture(name:String):void
        {
            var bitmap:BitmapData = new BitmapData(1280, 720, false, 0x08151C);
            bitmap.draw(menu, new Matrix(2 / 3, 0, 0, 2 / 3));
            var pixels:ByteArray = new ByteArray();
            for (var y:int = 0; y < 720; ++y) {
                pixels.writeByte(0);
                for (var x:int = 0; x < 1280; ++x) pixels.writeUnsignedInt((bitmap.getPixel(x, y) << 8) | 255);
            }
            bitmap.dispose(); pixels.compress();
            var png:ByteArray = new ByteArray(); png.writeUnsignedInt(0x89504E47); png.writeUnsignedInt(0x0D0A1A0A);
            var header:ByteArray = new ByteArray(); header.writeUnsignedInt(1280); header.writeUnsignedInt(720);
            header.writeByte(8); header.writeByte(6); header.writeByte(0); header.writeByte(0); header.writeByte(0);
            chunk(png, "IHDR", header); chunk(png, "IDAT", pixels); chunk(png, "IEND", new ByteArray());
            var alphabet:String = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            var encoded:String = "";
            for (var i:int = 0; i < png.length; i += 3) {
                var bits:uint = (png[i] << 16) | ((i + 1 < png.length ? png[i + 1] : 0) << 8) | (i + 2 < png.length ? png[i + 2] : 0);
                encoded += alphabet.charAt((bits >> 18) & 63) + alphabet.charAt((bits >> 12) & 63) +
                    (i + 1 < png.length ? alphabet.charAt((bits >> 6) & 63) : "=") + (i + 2 < png.length ? alphabet.charAt(bits & 63) : "=");
            }
            trace("[preview-png:" + name + "] " + encoded);
        }
        private function chunk(png:ByteArray, type:String, data:ByteArray):void
        {
            png.writeUnsignedInt(data.length);
            var start:uint = png.position; png.writeUTFBytes(type); png.writeBytes(data);
            var crc:uint = 0xFFFFFFFF;
            for (var i:uint = start; i < png.position; ++i) {
                crc ^= png[i];
                for (var bit:int = 0; bit < 8; ++bit) crc = (crc >>> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
            png.writeUnsignedInt(crc ^ 0xFFFFFFFF);
        }
    }
}
