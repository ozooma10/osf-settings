package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    public final class KeyboardMap extends Sprite
    {
        private var keys:Array = [];
        private var choose:Function;
        private var previous:String = "";
        private static var names:Object = {};
        public function KeyboardMap(select:Function)
        {
            choose = select;
            addKey(27, Localization.text("keys.esc"), 0, 0);
            for (var i:int = 0; i < 12; ++i) addKey(112 + i, "F" + (i + 1), 2 + i + int(i / 4) * .5, 0);
            addKey(44,Localization.text("keys.prtsc"),15.5,0); addKey(145,Localization.text("keys.scroll"),16.5,0); addKey(19,Localization.text("keys.pause"),17.5,0);
            row([192,49,50,51,52,53,54,55,56,57,48,189,187], ["`","1","2","3","4","5","6","7","8","9","0","-","="],0,1);
            addKey(8,Localization.text("keys.backspace"),13,1,2);
            addKey(9,Localization.text("keys.tab"),0,2,1.5); row([81,87,69,82,84,89,85,73,79,80,219,221],["Q","W","E","R","T","Y","U","I","O","P","[","]"],1.5,2); addKey(220,"\\",13.5,2,1.5);
            addKey(20,Localization.text("keys.caps"),0,3,1.75); row([65,83,68,70,71,72,74,75,76,186,222],["A","S","D","F","G","H","J","K","L",";","'"],1.75,3); addKey(13,Localization.text("keys.enter"),12.75,3,2.25);
            addKey(160,Localization.text("keys.lshift"),0,4,2.25); row([90,88,67,86,66,78,77,188,190,191],["Z","X","C","V","B","N","M",",",".","/"],2.25,4); addKey(161,Localization.text("keys.rshift"),12.25,4,2.75);
            addKey(162,Localization.text("keys.lctrl"),0,5,1.25); addKey(91,Localization.text("keys.win"),1.25,5,1.25); addKey(164,Localization.text("keys.lalt"),2.5,5,1.25); addKey(32,Localization.text("keys.space"),3.75,5,6.25); addKey(165,Localization.text("keys.ralt"),10,5,1.25); addKey(92,Localization.text("keys.win"),11.25,5,1.25); addKey(93,Localization.text("keys.menu"),12.5,5,1.25); addKey(163,Localization.text("keys.rctrl"),13.75,5,1.25);
            row([45,36,33],[Localization.text("keys.ins"),Localization.text("keys.home"),Localization.text("keys.pgup")],15.5,1); row([46,35,34],[Localization.text("keys.del"),Localization.text("keys.end"),Localization.text("keys.pgdn")],15.5,2);
            addKey(38,Localization.text("keys.up"),16.5,4); row([37,40,39],[Localization.text("keys.left"),Localization.text("keys.down"),Localization.text("keys.right")],15.5,5);
            row([144,111,106,109],[Localization.text("keys.num"),"/","*","-"],19,1);
            row([103,104,105],["7","8","9"],19,2); addKey(107,"+",22,2,1,2);
            row([100,101,102],["4","5","6"],19,3); row([97,98,99],["1","2","3"],19,4); addKey(13,Localization.text("keys.enter"),22,4,1,2);
            addKey(96,"0",19,5,2); addKey(110,".",21,5);
        }
        private function row(codes:Array, labels:Array, x:Number, y:Number):void
        {
            for (var i:int = 0; i < codes.length; ++i) addKey(codes[i],labels[i],x + i,y);
        }
        private function addKey(code:uint, text:String, column:Number, row:Number, width:Number = 1, height:Number = 1):void
        {
            var key:Sprite = new Sprite(); key.x = column * 75; key.y = row * 43;
            key.name = "key_" + code; key.buttonMode = true;
            var label:TextField = MenuStyle.field(text,4,4,width * 75 - 13,29,CONFIG::largeText ? 21 : 19,MenuStyle.WHITE,true);
            key.addChild(label); key.addEventListener(MouseEvent.CLICK,function(event:MouseEvent):void { choose(int(code)); });
            keys.push({clip:key,code:code,label:label,width:width * 75 - 6,height:height * 43 - 5}); addChild(key);
            if (code >= 96 && code <= 111) names[code] = Localization.text("keys.numpad", {key:text});
            else names[code] = text;
        }
        public static function keyName(code:uint, device:uint):String
        {
            if (!KeybindingsData.bound(code)) return Localization.text("values.unboundTitle");
            if (device == 1) return Localization.text("keys.mouse", {number:code + 1});
            if (code == 16) return Localization.text("keys.shift"); if (code == 17) return Localization.text("keys.ctrl"); if (code == 18) return Localization.text("keys.alt");
            return names[code] || Localization.text("keys.code", {code:code});
        }
        public function update(rows:Array, visibleRows:Array, selected:Object, filter:int):void
        {
            var signature:String = filter + ":" + (selected ? selected.identity : "") + ":";
            for each (var row:Object in rows) for each (var record:Object in row.records)
                signature += row.identity + "," + record.device + "," + record.slot + "," + record.key + "," + record.modifier + ";";
            for each (row in visibleRows) signature += row.identity + "|";
            if (signature == previous) return;
            previous = signature;
            for each (var key:Object in keys) {
                var owners:Object = {}, game:Boolean = false, mod:Boolean = false, shown:Boolean = false, active:Boolean = false, total:int = 0;
                for each (row in rows) {
                    for each (record in row.records) if (KeybindingsData.includes(record,key.code)) {
                        owners[row.identity] = true;
                        if (row.mod) mod = true; else game = true;
                        if (selected && row.identity == selected.identity) active = true;
                        if (visibleRows.indexOf(row) >= 0) shown = true;
                    }
                }
                for (var id:String in owners) ++total;
                var clip:Sprite = key.clip;
                clip.alpha = total == 0 || shown ? 1 : .45;
                clip.graphics.clear(); clip.graphics.lineStyle(active || filter == key.code ? 3 : 1, filter == key.code ? MenuStyle.ACCENT : active ? MenuStyle.WHITE : MenuStyle.LINE);
                clip.graphics.beginFill(active ? MenuStyle.WHITE : MenuStyle.ROW); clip.graphics.drawRect(0,0,key.width,key.height); clip.graphics.endFill();
                key.label.textColor = active ? MenuStyle.INK : MenuStyle.WHITE;
                if (total) {
                    clip.graphics.lineStyle(); clip.graphics.beginFill(mod ? MenuStyle.ACCENT : MenuStyle.MUTED);
                    clip.graphics.drawRect(4,key.height - 5,game && mod ? (key.width - 8) / 2 : key.width - 8,3); clip.graphics.endFill();
                    if (game && mod) { clip.graphics.beginFill(MenuStyle.MUTED); clip.graphics.drawRect(key.width / 2,key.height - 5,key.width / 2 - 4,3); clip.graphics.endFill(); }
                    if (total > 1) MenuStyle.diamond(clip.graphics,key.width - 7,7,active ? MenuStyle.INK : MenuStyle.ACCENT);
                }
            }
        }
    }
}
