package
{
    // The vanilla slider uses integer positions. Float coordinates arrive scaled
    // from native code so decimal increments never accumulate binary rounding.
    public final class NumericSetting
    {
        public static function isNumeric(row:Object):Boolean
        {
            return row != null && (row.type == "int" || row.type == "float");
        }
        public static function isSlider(row:Object):Boolean { return isNumeric(row) && row.editable; }
        public static function steps(row:Object):Number
        {
            return row.type == "float" ? Number(row.sliderSteps) : Number(row.maximum) - Number(row.minimum);
        }
        public static function position(row:Object):Number
        {
            if (row.type == "int") return Number(row.value) - Number(row.minimum);
            if (Number(row.value) >= Number(row.sliderMaximum) / Number(row.sliderScale)) return steps(row);
            var position:Number = (Number(row.value) * Number(row.sliderScale) - Number(row.sliderMinimum)) / Number(row.sliderStep);
            position = Math.max(0, Math.min(steps(row), Math.round(position)));
            // Near Number's precision limit, multiplying a saved decimal back by
            // its scale can move the estimate. Recover its exact neighboring tick.
            if (atPosition(row, position) != row.value) {
                if (position > 0 && atPosition(row, position - 1) == row.value) return position - 1;
                if (position < steps(row) && atPosition(row, position + 1) == row.value) return position + 1;
            }
            return position;
        }
        public static function atPosition(row:Object, position:Number):*
        {
            position = Math.max(0, Math.min(steps(row), Math.round(position)));
            if (row.type == "int") return String(Number(row.minimum) + position);
            var value:Number = position == steps(row) ? Number(row.sliderMaximum) :
                Number(row.sliderMinimum) + position * Number(row.sliderStep);
            return value / Number(row.sliderScale);
        }
        public static function text(row:Object, value:*):String
        {
            if (row.type == "bool") return value ? tr("values.on") : tr("values.off");
            if (row.type == "key") {
                if (Number(value) == 255) return tr("values.unbound");
                if (value == row.value && row.valueName) return String(row.valueName);
                if (value == row.defaultValue && row.defaultName) return String(row.defaultName);
                return tr("keys.hex", {code:uint(value).toString(16).toUpperCase()});
            }
            if (row.type == "float" && row.decimals >= 0) {
                var fixed:String = Number(value).toFixed(int(row.decimals));
                // Preserve an off-step saved value or default in the display.
                if (Number(fixed) == Number(value)) return fixed;
            }
            return String(value);
        }
    }
}
