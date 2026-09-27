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
        private static const BOX_WIDTH:Number = 880;
        private static const BOX_HEIGHT:Number = CONFIG::largeText ? 480 : 430;
        private static const BOX_X:Number = (1920 - BOX_WIDTH) / 2;
        private static const BOX_Y:Number = (1080 - BOX_HEIGHT) / 2;
        private static const CHOICE_WIDTH:Number = (BOX_WIDTH - 72 - 20) / 2;
        private static const CHOICE_HEIGHT:Number = CONFIG::largeText ? 56 : 50;

        public function ActionConfirmation(finished:Function)
        {
            done = finished;
            graphics.beginFill(0, 0.85); graphics.drawRect(-4096, -4096, 10000, 10000); graphics.endFill();
            graphics.beginFill(MenuStyle.ROW); graphics.lineStyle(1, MenuStyle.LINE);
            // A compact centered box; the message scrolls when it outgrows it.
            graphics.drawRect(BOX_X, BOX_Y, BOX_WIDTH, BOX_HEIGHT); graphics.endFill();
            var left:Number = BOX_X + 36, inner:Number = BOX_WIDTH - 72;
            heading = MenuStyle.field("", left, BOX_Y + 30, inner, MenuStyle.DETAIL_TITLE_SIZE + 20, MenuStyle.DETAIL_TITLE_SIZE + 3);
            addChild(heading);
            message = MenuStyle.field("", left, BOX_Y + 96, inner, BOX_HEIGHT - 226, MenuStyle.DETAIL_BODY_SIZE, MenuStyle.MUTED);
            message.multiline = message.wordWrap = true; addChild(message);
            addChild(MenuStyle.field(tr("actions.confirmControls"), left, BOX_Y + BOX_HEIGHT - 46, inner, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE, MenuStyle.MUTED));
            for (var i:int = 0; i < 2; ++i) {
                var choice:Sprite = new Sprite(); choice.x = left + i * (CHOICE_WIDTH + 20); choice.y = BOX_Y + BOX_HEIGHT - 116;
                choice.name = String(i); choice.buttonMode = true; choice.mouseChildren = false;
                var text:TextField = MenuStyle.field(i == 0 ? tr("buttons.cancel") : tr("buttons.runAction"), 18, (CHOICE_HEIGHT - MenuStyle.DETAIL_BODY_SIZE) / 2 - 4,
                    CHOICE_WIDTH - 36, MenuStyle.DETAIL_BODY_SIZE + 14, MenuStyle.DETAIL_BODY_SIZE);
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
                choice.graphics.drawRect(0, 0, CHOICE_WIDTH, CHOICE_HEIGHT); choice.graphics.endFill();
                TextField(choice.getChildAt(0)).textColor = i == selected ? MenuStyle.INK : MenuStyle.WHITE;
            }
        }
    }
}
