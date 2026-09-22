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
            return value.replace(/\{([A-Za-z0-9_]+)\}/g, function(...match):String {
                return parameters.hasOwnProperty(match[1]) ? String(parameters[match[1]]) : match[0];
            });
        }
    }
}
