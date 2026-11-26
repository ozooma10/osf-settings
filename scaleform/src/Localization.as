package
{
    public final class Localization
    {
        private static var messages:Object = English.ui;

        public static function initialize(values:Object):void { messages = values || English.ui; }

        public static function text(key:String, parameters:Object = null):String
        {
            var value:String = messages.hasOwnProperty(key) ? String(messages[key]) :
                English.ui.hasOwnProperty(key) ? String(English.ui[key]) : key;
            if (!parameters) return value;
            // Scaleform RegExp offsets can count UTF-8 bytes while substring uses characters.
            // Scan with string indices; also avoid its unreliable String.replace callbacks.
            // Inserted values remain literal and unknown tokens remain intact.
            var result:String = "";
            var offset:int = 0;
            var start:int = value.indexOf("{", offset);
            while (start >= 0) {
                var end:int = start + 1;
                while (end < value.length) {
                    var code:int = value.charCodeAt(end);
                    if (!((code >= 65 && code <= 90) || (code >= 97 && code <= 122) ||
                        (code >= 48 && code <= 57) || code == 95)) break;
                    ++end;
                }
                if (end > start + 1 && value.charAt(end) == "}") {
                    var name:String = value.substring(start + 1, end);
                    result += value.substring(offset, start);
                    result += parameters.hasOwnProperty(name) ? String(parameters[name]) : value.substring(start, end + 1);
                    offset = end + 1;
                    start = value.indexOf("{", offset);
                } else {
                    start = value.indexOf("{", start + 1);
                }
            }
            return result + value.substring(offset);
        }
    }
}
