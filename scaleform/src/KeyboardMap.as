package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;
    import flash.utils.Dictionary;

    public final class KeyboardMap extends Sprite
    {
        // One key unit; the full map is 23 units wide and 6 rows tall, which must fit
        // between the navigation column and the right margin.
        public static const KEY_WIDTH:Number = 62;
        public static const KEY_HEIGHT:Number = CONFIG::largeText ? 40 : 34;
        public static const HEIGHT:Number = KEY_HEIGHT * 6;
        private static const LABEL_SIZE:Number = CONFIG::largeText ? 18 : 15;
        private var keys:Array = [];
        private var choose:Function;
        private var previousRows:Array;
        private var previousVisible:Array;
        private var previousSelection:Object;
        private var previousFilter:int;
        private static var names:Object = {};
        public function KeyboardMap(select:Function)
        {
            choose = select;
            addKey(27, tr("keys.esc"), 0, 0);
            for (var i:int = 0; i < 12; ++i) addKey(112 + i, "F" + (i + 1), 2 + i + int(i / 4) * .5, 0);
            addKey(44,tr("keys.prtsc"),15.5,0); addKey(145,tr("keys.scroll"),16.5,0); addKey(19,tr("keys.pause"),17.5,0);
            row([192,49,50,51,52,53,54,55,56,57,48,189,187], ["`","1","2","3","4","5","6","7","8","9","0","-","="],0,1);
            addKey(8,tr("keys.backspace"),13,1,2);
            addKey(9,tr("keys.tab"),0,2,1.5); row([81,87,69,82,84,89,85,73,79,80,219,221],["Q","W","E","R","T","Y","U","I","O","P","[","]"],1.5,2); addKey(220,"\\",13.5,2,1.5);
            addKey(20,tr("keys.caps"),0,3,1.75); row([65,83,68,70,71,72,74,75,76,186,222],["A","S","D","F","G","H","J","K","L",";","'"],1.75,3); addKey(13,tr("keys.enter"),12.75,3,2.25);
            addKey(160,tr("keys.lshift"),0,4,2.25); row([90,88,67,86,66,78,77,188,190,191],["Z","X","C","V","B","N","M",",",".","/"],2.25,4); addKey(161,tr("keys.rshift"),12.25,4,2.75);
            addKey(162,tr("keys.lctrl"),0,5,1.25); addKey(91,tr("keys.win"),1.25,5,1.25); addKey(164,tr("keys.lalt"),2.5,5,1.25); addKey(32,tr("keys.space"),3.75,5,6.25); addKey(165,tr("keys.ralt"),10,5,1.25); addKey(92,tr("keys.win"),11.25,5,1.25); addKey(93,tr("keys.menu"),12.5,5,1.25); addKey(163,tr("keys.rctrl"),13.75,5,1.25);
            row([45,36,33],[tr("keys.ins"),tr("keys.home"),tr("keys.pgup")],15.5,1); row([46,35,34],[tr("keys.del"),tr("keys.end"),tr("keys.pgdn")],15.5,2);
            addKey(38,tr("keys.up"),16.5,4); row([37,40,39],[tr("keys.left"),tr("keys.down"),tr("keys.right")],15.5,5);
            row([144,111,106,109],[tr("keys.num"),"/","*","-"],19,1);
            row([103,104,105],["7","8","9"],19,2); addKey(107,"+",22,2,1,2);
            row([100,101,102],["4","5","6"],19,3); row([97,98,99],["1","2","3"],19,4); addKey(13,tr("keys.enter"),22,4,1,2);
            addKey(96,"0",19,5,2); addKey(110,".",21,5);
            // The navigation cluster's empty row holds the legend.
            legendItem(tr("bindings.game"),MenuStyle.MUTED,15.5,3); legendItem(tr("bindings.mod"),MenuStyle.ACCENT,17,3);
        }
        private function legendItem(text:String, color:uint, column:Number, row:Number):void
        {
            graphics.beginFill(color); graphics.drawRect(column * KEY_WIDTH + 6,row * KEY_HEIGHT + KEY_HEIGHT / 2 - 3,18,3); graphics.endFill();
            var label:TextField = MenuStyle.field("",column * KEY_WIDTH + 28,row * KEY_HEIGHT + 3,KEY_WIDTH * (column == 17 ? 1.1 : 1) - 2,KEY_HEIGHT - 8,LABEL_SIZE - 1,MenuStyle.MUTED,true);
            addChild(label); MenuStyle.fit(label,text);
        }
        private function row(codes:Array, labels:Array, x:Number, y:Number):void
        {
            for (var i:int = 0; i < codes.length; ++i) addKey(codes[i],labels[i],x + i,y);
        }
        private function addKey(code:uint, text:String, column:Number, row:Number, width:Number = 1, height:Number = 1):void
        {
            var key:Sprite = new Sprite(); key.x = column * KEY_WIDTH; key.y = row * KEY_HEIGHT;
            key.name = "key_" + code; key.buttonMode = true;
            var label:TextField = MenuStyle.field(text,3,3,width * KEY_WIDTH - 11,KEY_HEIGHT - 8,LABEL_SIZE,MenuStyle.WHITE,true);
            key.addChild(label); key.addEventListener(MouseEvent.CLICK,function(event:MouseEvent):void { choose(int(code)); });
            keys.push({clip:key,code:code,label:label,width:width * KEY_WIDTH - 5,height:height * KEY_HEIGHT - 4}); addChild(key);
            if (code >= 96 && code <= 111) names[code] = tr("keys.numpad", {key:text});
            else names[code] = text;
        }
        public static function keyName(code:uint, device:uint):String
        {
            if (!KeybindingsData.bound(code)) return tr("values.unboundTitle");
            if (device == 2) return ControllerButtons.keyName(code);
            if (device == 1) return tr("keys.mouse", {number:code + 1});
            if (code == 16) return tr("keys.shift"); if (code == 17) return tr("keys.ctrl"); if (code == 18) return tr("keys.alt");
            return names[code] || tr("keys.code", {code:code});
        }
        public function update(rows:Array, visibleRows:Array, selected:Object, filter:int):void
        {
            if (previousRows == rows && previousVisible == visibleRows && previousSelection == selected && previousFilter == filter) return;
            previousRows = rows; previousVisible = visibleRows; previousSelection = selected; previousFilter = filter;
            var visibleSet:Dictionary = new Dictionary(), usage:Object = {};
            for each (var row:Object in visibleRows) visibleSet[row] = true;
            // Accumulate usage once instead of scanning every binding for every key.
            for each (row in rows) {
                var flags:uint = 1 | (row.mod ? 2 : 0) | (visibleSet[row] ? 4 : 0) | (row == selected ? 8 : 0);
                for each (var record:Object in row.records) {
                    if (!KeybindingsData.bound(record.key)) continue;
                    var recordFlags:uint = flags | (record.visibleInControls === false ? 32 : 0);
                    if (record.device == 0) mark(usage, record.key, recordFlags);
                    if (record.device != 2 && KeybindingsData.modified(record.modifier)) mark(usage, record.modifier, recordFlags);
                }
            }
            for each (var key:Object in keys) {
                flags = uint(usage[key.code]) | (filter == key.code ? 16 : 0);
                if (key.state === flags) continue;
                key.state = flags;
                var owned:Boolean = Boolean(flags & 1), mod:Boolean = Boolean(flags & 2), shown:Boolean = Boolean(flags & 4);
                var active:Boolean = Boolean(flags & 8), filtered:Boolean = Boolean(flags & 16);
                var reserved:Boolean = Boolean(flags & 32);
                var clip:Sprite = key.clip;
                // Keys used by the visible results stay bright; free and filtered-out keys recede.
                clip.alpha = shown || active || filtered ? 1 : .45;
                clip.graphics.clear();
                // The ring sits in the gap between keys so it still shows on a selected white key.
                if (filtered) { clip.graphics.lineStyle(2,MenuStyle.WHITE); clip.graphics.drawRect(-3,-3,key.width + 6,key.height + 6); }
                clip.graphics.lineStyle(1,active ? MenuStyle.WHITE : reserved ? MenuStyle.ROW : MenuStyle.LINE);
                clip.graphics.beginFill(active ? MenuStyle.WHITE : reserved ? MenuStyle.INK : MenuStyle.ROW); clip.graphics.drawRect(0,0,key.width,key.height); clip.graphics.endFill();
                key.label.textColor = active ? MenuStyle.INK : reserved ? MenuStyle.RESERVED : MenuStyle.WHITE;
                if (owned) {
                    clip.graphics.lineStyle(); clip.graphics.beginFill(reserved ? MenuStyle.RESERVED : mod ? MenuStyle.ACCENT : MenuStyle.MUTED);
                    clip.graphics.drawRect(4,key.height - 5,key.width - 8,3); clip.graphics.endFill();
                }
            }
        }
        private function mark(usage:Object, code:uint, flags:uint):void
        {
            usage[code] = uint(usage[code]) | flags;
            // Generic modifier records light both physical keys, like includes().
            if (code >= 16 && code <= 18) {
                var left:uint = 160 + (code - 16) * 2;
                usage[left] = uint(usage[left]) | flags;
                usage[left + 1] = uint(usage[left + 1]) | flags;
            }
        }
    }
}
