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
            // Scaleform can stringify a String.replace callback instead of invoking it.
            // Copy matches explicitly, leaving inserted values literal and unknown tokens intact.
            var pattern:RegExp = /\{([A-Za-z0-9_]+)\}/g;
            var result:String = "";
            var offset:int = 0;
            var match:Object;
            while ((match = pattern.exec(value)) != null) {
                result += value.substring(offset, match.index);
                result += parameters.hasOwnProperty(match[1]) ? String(parameters[match[1]]) : match[0];
                offset = match.index + String(match[0]).length;
            }
            return result + value.substring(offset);
        }
    }
}
