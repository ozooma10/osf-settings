package
{
    // Option positions belong to the vanilla stepper. Values identify saved choices.
    public final class EnumSetting
    {
        public static function index(row:Object):int
        {
            for (var i:int = 0; i < row.options.length; ++i)
                if (row.options[i].value === row.value) return i;
            return -1;
        }
        public static function labels(row:Object):Array
        {
            var result:Array = [];
            for each (var option:Object in row.options) result.push(option.label);
            return result;
        }
        public static function text(row:Object, value:*):String
        {
            for each (var option:Object in row.options)
                if (option.value === value) return String(option.label);
            return String(value);
        }
    }
}
