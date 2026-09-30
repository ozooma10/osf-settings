package
{
    public final class ControllerButtons
    {
        private static const TOKENS:Object = {9:"L2", 256:"L1", 512:"R1", 10:"R2", 32:"Select", 16:"Start",
            64:"L3", 128:"R3", 1:"DPad_Up", 4:"DPad_Left", 8:"DPad_Right", 2:"DPad_Down",
            32768:"Y", 16384:"X", 8192:"B", 4096:"A"};
        public static function keyName(code:uint):String
        {
            if (TOKENS[code]) return tokenName(TOKENS[code]);
            return tr("keys.code", {code:code});
        }
        public static function tokenName(token:String):String
        {
            token = token.replace(/^(Xenon_|PSN_)/, "");
            var key:String = "keys.pad" + token;
            var value:String = tr(key);
            return value == key ? token.replace(/_/g, " ") : value;
        }
    }
}
