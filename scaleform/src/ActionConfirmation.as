package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;

    // Modal confirmation with Cancel selected initially and a fresh press/release to accept.
    public final class ActionConfirmation extends Sprite
    {
        public var row:Object;
        private var heading:TextField;
        private var message:TextField;
        private var choices:Array = [];
        private var selected:int = 0;
        private var acceptBlocked:Boolean;
        private var acceptDown:Boolean;
        private var mouseDownChoice:Object;
        private var done:Function;

        public function ActionConfirmation(finished:Function)
        {
            done = finished;
            graphics.beginFill(0, 0.85); graphics.drawRect(-4096, -4096, 10000, 10000); graphics.endFill();
            graphics.beginFill(MenuStyle.ROW); graphics.lineStyle(1, MenuStyle.LINE);
            graphics.drawRect(410, 230, 1100, 610); graphics.endFill();
            heading = MenuStyle.field("", 450, 284, 1020, 68, CONFIG::largeText ? 38 : 34);
            addChild(heading);
            message = MenuStyle.field("", 450, 390, 1020, 280, CONFIG::largeText ? 30 : 27, MenuStyle.MUTED);
            message.multiline = message.wordWrap = true; addChild(message);
            addChild(MenuStyle.field(tr("actions.confirmControls"), 450, 795, 1020, 32, 20, MenuStyle.MUTED));
            for (var i:int = 0; i < 2; ++i) {
                var choice:Sprite = new Sprite(); choice.x = 450 + i * 530; choice.y = 708;
                choice.name = String(i); choice.buttonMode = true; choice.mouseChildren = false;
                var text:TextField = MenuStyle.field(i == 0 ? tr("buttons.cancel") : tr("buttons.runAction"), 20, 12, 450, 44, CONFIG::largeText ? 30 : 27);
                choice.addChild(text); choices.push(choice); addChild(choice);
                choice.addEventListener(MouseEvent.MOUSE_DOWN, mousePress);
                choice.addEventListener(MouseEvent.CLICK, mouseClick);
            }
            addEventListener(MouseEvent.MOUSE_WHEEL, scroll);
            visible = false;
        }
        public function open(value:Object, held:Boolean):void
        {
            row = value; selected = 0; acceptBlocked = held; acceptDown = false; mouseDownChoice = null;
            MenuStyle.fit(heading, value.title); MenuStyle.setText(message, value.confirmation);
            message.scrollV = 1; visible = true; draw();
        }
        public function cancel():void { if (visible) finish(false); }
        private function finish(run:Boolean):void
        {
            var action:Object = row;
            visible = false; row = null; acceptDown = false; mouseDownChoice = null;
            done(action, run);
        }
        public function userEvent(name:String, pressed:Boolean):Boolean
        {
            if (!visible) return false;
            if (name == "Accept") {
                if (pressed) { if (!acceptBlocked) acceptDown = true; }
                else {
                    var choose:Boolean = !acceptBlocked && acceptDown;
                    acceptBlocked = acceptDown = false;
                    if (choose) finish(selected == 1);
                }
            } else if (name == "Cancel") {
                if (!pressed) finish(false);
            } else if (pressed && (name == "Left" || name == "Right")) {
                selected = name == "Left" ? 0 : 1; draw();
            } else if (pressed && (name == "Up" || name == "Down")) {
                message.scrollV += name == "Up" ? -1 : 1;
            }
            return true;
        }
        private function mousePress(event:MouseEvent):void { mouseDownChoice = event.currentTarget; event.stopPropagation(); }
        private function mouseClick(event:MouseEvent):void
        {
            event.stopPropagation();
            if (mouseDownChoice == event.currentTarget) finish(event.currentTarget.name == "1");
        }
        private function scroll(event:MouseEvent):void { message.scrollV -= event.delta; event.stopPropagation(); }
        private function draw():void
        {
            for (var i:int = 0; i < choices.length; ++i) {
                var choice:Sprite = choices[i]; choice.graphics.clear();
                choice.graphics.beginFill(i == selected ? MenuStyle.WHITE : MenuStyle.INK);
                choice.graphics.drawRect(0, 0, 490, 64); choice.graphics.endFill();
                TextField(choice.getChildAt(0)).textColor = i == selected ? MenuStyle.INK : MenuStyle.WHITE;
            }
        }
    }
}
