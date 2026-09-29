package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    public final class GamepadMap extends Sprite
    {
        // Native button IDs, including Starfield's two trigger button events.
        private static const CODES:Array = [256,9,16,32,10,512,64,128,1,2,4,8,4096,8192,16384,32768];
        private static const LABELS:Array = ["LB","LT","Menu","View","RT","RB","LS","RS",
            "D-pad Up","D-pad Down","D-pad Left","D-pad Right","A","B","X","Y"];
        private var buttons:Array = [];
        public function GamepadMap(select:Function)
        {
            var heading:TextField = MenuStyle.field(tr("bindings.controller"),0,0,1000,40,MenuStyle.VALUE_SIZE,MenuStyle.WHITE,true);
            addChild(heading);
            for (var i:int = 0; i < CODES.length; ++i) addButton(i,select);
            var hint:TextField = MenuStyle.field(tr("bindings.deviceHint"),0,164,1420,64,MenuStyle.SMALL_SIZE,MenuStyle.MUTED,true);
            hint.wordWrap = true; hint.multiline = true; addChild(hint);
        }
        public static function buttonName(code:uint):String
        {
            if (!KeybindingsData.bound(code)) return tr("values.unboundTitle");
            var index:int = CODES.indexOf(code);
            return index >= 0 ? LABELS[index] : tr("keys.code", {code:code});
        }
        private function addButton(index:int, select:Function):void
        {
            var code:uint = CODES[index];
            var button:Sprite = new Sprite(); button.x = index % 8 * 178; button.y = 48 + int(index / 8) * 56;
            button.name = "gamepad_" + code; button.buttonMode = true;
            var label:TextField = MenuStyle.field(LABELS[index],8,8,152,32,MenuStyle.SMALL_SIZE,MenuStyle.WHITE,true);
            label.mouseEnabled = false; button.addChild(label);
            button.addEventListener(MouseEvent.CLICK,function(event:MouseEvent):void { select(int(code)); });
            buttons.push({clip:button,label:label,code:code}); addChild(button);
        }
        public function update(rows:Array, selected:Object, filter:int):void
        {
            for each (var button:Object in buttons) {
                var owned:Boolean = false, mod:Boolean = false, active:Boolean = false;
                for each (var row:Object in rows) for each (var record:Object in row.records) {
                    if (!KeybindingsData.includes(record,button.code,true)) continue;
                    owned = true; if (row.mod) mod = true;
                    if (selected && selected.identity == row.identity) active = true;
                }
                var clip:Sprite = button.clip;
                clip.graphics.clear();
                clip.graphics.lineStyle(filter == button.code ? 2 : 1,filter == button.code || active ? MenuStyle.WHITE : MenuStyle.LINE);
                clip.graphics.beginFill(active ? MenuStyle.WHITE : MenuStyle.ROW); clip.graphics.drawRect(0,0,168,46); clip.graphics.endFill();
                button.label.textColor = active ? MenuStyle.INK : MenuStyle.WHITE;
                if (owned) {
                    clip.graphics.lineStyle(); clip.graphics.beginFill(mod ? MenuStyle.ACCENT : MenuStyle.MUTED);
                    clip.graphics.drawRect(4,41,160,3); clip.graphics.endFill();
                }
            }
        }
    }
}
