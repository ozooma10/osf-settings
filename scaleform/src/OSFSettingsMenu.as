package
{
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.display.Stage;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;
    import flash.text.TextField;
    import flash.text.TextFormat;
    import flash.ui.Keyboard;
    import flash.utils.Dictionary;
    import flash.utils.getDefinitionByName;

    [SWF(width="1920", height="1080", frameRate="60", backgroundColor="#000000")]
    public final class OSFSettingsMenu extends MovieClip
    {
        public var BGSCodeObj:Object = {};
        public var startupPhase:String = "document constructed";
        private var bridgeReady:Boolean = false;
        private var initialized:Boolean = false;
        private var starting:Boolean = false;
        private var closing:Boolean = false;
        private var refreshing:Boolean = false;
        private var requestedRefresh:Boolean = false;
        private var frame:int = 0;
        private var activationFrame:int = -1;
        private var rows:Array = [];
        private var options:Object;
        private var types:Class;
        private var bar:Object;
        private var background:MovieClip;
        private var menuStage:Stage;
        private var detailTitle:TextField;
        private var detailValue:TextField;
        private var detailHint:TextField;
        private var status:TextField;
        private var bodyFormat:TextFormat;
        private var smallFormat:TextFormat;
        private var clipLayout:Dictionary = new Dictionary(true);
        private static const LEFT:Number = 160;
        private static const WIDTH:Number = 1580;

        public function OSFSettingsMenu()
        {
            addEventListener(Event.ADDED_TO_STAGE, added);
            addEventListener(Event.REMOVED_FROM_STAGE, removed);
        }
        private function definition(name:String):Class
        {
            if (!initialized) startupPhase = "resolve " + name;
            var result:Class = getDefinitionByName(name) as Class;
            if (!result) throw new Error("Missing game class: " + name);
            return result;
        }
        private function create(name:String):Object { var c:Class = definition(name); return new c(); }
        public function onCodeObjCreate():void
        {
            bridgeReady = true; BGSCodeObj.startup("native bridge ready"); initializeWhenReady();
        }
        private function added(event:Event):void { if (event.target == this) initializeWhenReady(); }
        private function initializeWhenReady():void
        {
            if (!bridgeReady || !stage || initialized || starting) return;
            starting = true;
            try {
                menuStage = stage; buildMenu(); initialized = true;
                startupPhase = "populate settings"; refresh(false);
                startupPhase = "ready"; BGSCodeObj.startup(startupPhase);
            } catch (error:Error) {
                initialized = false; closing = true;
                BGSCodeObj.startupFailed(startupPhase + ": " + error.toString());
            }
        }
        private function buildMenu():void
        {
            // Use the game's components without loading its SettingsPanel controller.
            background = create("FullBackground") as MovieClip;
            background.mouseEnabled = false; background.mouseChildren = false; addChild(background);
            types = definition("Shared.Components.SystemPanels.SettingsOptionListEntry");
            bodyFormat = gameFormat("$NB_Grotesk_Semibold", CONFIG::largeText ? 28 : 24, 0xEDEDED);
            smallFormat = gameFormat("$NB_Grotesk_Semibold", CONFIG::largeText ? 24 : 20, 0xBCC5CB);
            var chrome:Sprite = new Sprite(); chrome.mouseEnabled = false; addChild(chrome);
            chrome.graphics.lineStyle(1, 0x64717A, 0.7);
            chrome.graphics.moveTo(LEFT, 150); chrome.graphics.lineTo(LEFT + WIDTH, 150);
            chrome.graphics.beginFill(0x18242C); chrome.graphics.drawRect(LEFT, 770, WIDTH, 160); chrome.graphics.endFill();
            chrome.graphics.moveTo(LEFT, 980); chrome.graphics.lineTo(LEFT + WIDTH, 980);
            addChild(field("MOD SETTINGS", LEFT, 65, WIDTH, 65, gameFormat("$MAIN_Font_Bold", CONFIG::largeText ? 40 : 36, 0xFFFFFF)));
            options = create("Shared.Components.SystemPanels.SettingsOptionList");
            options.x = LEFT; options.y = 200; addChild(options as MovieClip);
            var config:Object = create("Shared.AS3.BSScrollingConfigParams");
            config.EntryClassName = "OptionListEntry"; config.VerticalSpacing = CONFIG::largeText ? 12 : 10;
            config.TruncateToFit = true; config.RestoreIndex = true; config.WrapAround = false;
            options.Configure(config); options.Border_mc.width = WIDTH; options.borderHeight = 535; options.scrollBarHeight = 535;
            if (options.ScrollBar) options.ScrollBar.x = WIDTH + 8;
            options.addEventListener("ScrollingEvent::selectionChange", selectionChanged);
            options.addEventListener("ScrollingEvent::itemPress", itemPressed);
            options.addEventListener("ScrollingEvent::playFocusSound", focusSound);
            options.addEventListener("SettingsOptionEntry_ValueChanged", valueChanged);
            detailTitle = field("", LEFT + 20, 782, WIDTH - 40, 40, bodyFormat); addChild(detailTitle);
            detailValue = field("", LEFT + 20, 824, WIDTH - 40, 34, smallFormat); addChild(detailValue);
            detailHint = field("", LEFT + 20, 862, WIDTH - 40, 64, smallFormat);
            detailHint.multiline = true; detailHint.wordWrap = true; addChild(detailHint);
            status = field("Changes are saved automatically.", LEFT, 942, WIDTH, 35, smallFormat); addChild(status);
            bar = create("Shared.Components.ButtonControls.ButtonBar.ButtonBar");
            bar.x = LEFT; bar.y = 1000; addChild(bar as MovieClip); bar.Initialize(0, 24);
            button("TOGGLE", "Accept", accept); button("$BACK", "Cancel", back); bar.RefreshButtons();
            menuStage.stageFocusRect = false;
            menuStage.addEventListener(Event.RESIZE, resizeBackground); resizeBackground();
            menuStage.addEventListener(KeyboardEvent.KEY_DOWN, keyDown, true);
            menuStage.addEventListener(KeyboardEvent.KEY_UP, keyUp, true);
            menuStage.addEventListener(MouseEvent.MOUSE_DOWN, mouseFocus, true);
            addEventListener(Event.ENTER_FRAME, advance);
        }
        private function button(label:String, userEvent:String, callback:Function):void
        {
            var eventClass:Class = definition("Shared.Components.ButtonControls.ButtonData.UserEventData");
            var dataClass:Class = definition("Shared.Components.ButtonControls.ButtonData.ButtonBaseData");
            var factory:Class = definition("Shared.Components.ButtonControls.ButtonFactory.ButtonFactory");
            Object(factory).AddToButtonBar("BasicButton", new dataClass(label, new eventClass(userEvent, callback)), bar);
        }
        private function resizeBackground(event:Event = null):void
        {
            var extensions:Class = definition("scaleform.gfx.Extensions"); Object(extensions).enabled = true;
            var visible:Rectangle = Object(extensions).visibleRect as Rectangle;
            if (!visible || visible.width <= 0 || visible.height <= 0) return;
            background.x = visible.x; background.y = visible.y; background.width = visible.width; background.height = visible.height;
        }
        private function removed(event:Event):void
        {
            if (event.target != this) return;
            if (menuStage) {
                menuStage.removeEventListener(Event.RESIZE, resizeBackground);
                menuStage.removeEventListener(KeyboardEvent.KEY_DOWN, keyDown, true);
                menuStage.removeEventListener(KeyboardEvent.KEY_UP, keyUp, true);
                menuStage.removeEventListener(MouseEvent.MOUSE_DOWN, mouseFocus, true);
            }
            removeEventListener(Event.ENTER_FRAME, advance);
        }
        private function refresh(preserve:Boolean = true):void
        {
            refreshing = true; requestedRefresh = false;
            var selected:int = preserve ? options.selectedIndex : 0;
            var scroll:int = preserve ? options.scrollPosition : 0;
            rows = BGSCodeObj.getRows() as Array || [];
            var data:Array = [];
            for (var i:int = 0; i < rows.length; ++i) {
                var row:Object = rows[i];
                data.push({row:row, sText:html(String(row.title)), uID:i, bDisabled:false, bShowSpinner:false,
                    uCategory:0, bEnabled:!row.header, bSubSetting:false, uType:row.header ? types.SDT_HEADER : types.SDT_CHECKBOX,
                    sliderData:{fValue:0, sDisplayValue:""}, stepperData:{aStepperOptions:[], uIndex:0},
                    checkBoxData:{bChecked:Boolean(row.value)}});
            }
            options.InitializeEntries(data);
            selected = selectableIndex(Math.max(0, Math.min(selected, rows.length - 1)), 1);
            options.selectedIndex = selected; options.scrollPosition = scroll;
            options.disableInput = false; menuStage.focus = options as MovieClip;
            refreshing = false; describe();
        }
        private function current():Object { return options && options.selectedEntry ? options.selectedEntry.row : null; }
        private function describe():void
        {
            var row:Object = current();
            detailTitle.text = row ? row.title : "No settings to display";
            detailValue.text = row && !row.header ? "Current: " + (row.value ? "On" : "Off") + "     Default: " + (row.defaultValue ? "On" : "Off") : "";
            detailHint.text = row ? String(row.hint || "") : "";
        }
        private function selectionChanged(event:Event):void { if (!refreshing) describe(); }
        private function focusSound(event:Event):void { Object(definition("Shared.GlobalFunc")).PlayMenuSound("UIMenuGeneralFocus"); }
        private function itemPressed(event:Event):void { accept(); }
        private function accept():void
        {
            if (closing || refreshing || requestedRefresh || activationFrame == frame || options.scrollbarScrolling) return;
            activationFrame = frame;
            var row:Object = current(); if (row && !row.header) options.OnEntryPressed();
        }
        private function valueChanged(event:Event):void
        {
            event.stopPropagation();
            if (closing || refreshing || requestedRefresh) return;
            var data:Object = Object(event).params;
            var item:Object = options.GetDataForEntry(int(data.id));
            if (item && !item.row.header) edit(item.row, Number(data.value) != 0);
        }
        private function edit(row:Object, value:Boolean):void
        {
            activationFrame = frame;
            var result:Object = BGSCodeObj.setBool(row.mod, row.key, value);
            status.text = result && result.ok ? "Changes are saved automatically." : result ? result.error : "Could not save this setting. Your previous value is unchanged.";
            // Re-read the store on the next frame, after the native control finishes
            // its event. This also restores the old checkbox when a save fails.
            requestedRefresh = true;
        }
        private function back():void
        {
            if (closing || options.scrollbarScrolling) return;
            closing = true; options.disableInput = true; BGSCodeObj.close();
        }
        public function ProcessUserEvent(name:String, pressed:Boolean):Boolean
        {
            return initialized && !closing && Boolean(bar.ProcessUserEvent(name, pressed));
        }
        private function mouseFocus(event:MouseEvent):void
        {
            if (!initialized || closing) return;
            var target:DisplayObject = event.target as DisplayObject;
            if (!MovieClip(options).contains(target)) return;
            menuStage.focus = options as MovieClip;
            while (target && target != options) {
                if ("itemIndex" in target) { options.selectedIndex = Object(target).itemIndex; break; }
                target = target.parent;
            }
        }
        private function keyDown(event:KeyboardEvent):void
        {
            if (!initialized || closing || refreshing || requestedRefresh || options.scrollbarScrolling) return;
            if (event.keyCode == Keyboard.UP || event.keyCode == Keyboard.DOWN) {
                var direction:int = event.keyCode == Keyboard.UP ? -1 : 1;
                var adjacent:int = options.selectedIndex + direction;
                if (adjacent >= 0 && adjacent < rows.length && rows[adjacent].header) {
                    event.stopImmediatePropagation(); event.preventDefault();
                    var next:int = selectableIndex(adjacent, direction); if (next >= 0) options.selectedIndex = next;
                }
            } else if (event.keyCode == Keyboard.LEFT || event.keyCode == Keyboard.RIGHT) {
                event.stopImmediatePropagation(); event.preventDefault();
                var row:Object = current(); if (row && !row.header) edit(row, event.keyCode == Keyboard.RIGHT);
            }
        }
        private function keyUp(event:KeyboardEvent):void
        {
            // The native Accept event already handles Enter; prevent a second
            // activation from the scrolling list's raw keyboard handler.
            if (event.keyCode == Keyboard.ENTER) event.stopImmediatePropagation();
        }
        private function selectableIndex(index:int, direction:int):int
        {
            while (index >= 0 && index < rows.length && rows[index].header) index += direction;
            return index >= 0 && index < rows.length ? index : -1;
        }
        private function advance(event:Event):void
        {
            ++frame; if (!initialized || closing) return;
            if (requestedRefresh && !options.scrollbarScrolling) refresh();
            decorate();
        }
        private function decorate():void
        {
            var needsLayout:Boolean = false;
            for (var i:int = 0; i < options.totalEntryClips; ++i) {
                var clip:Object = options.GetClipByIndex(i); if (!clip || clip.itemIndex < 0) continue;
                var item:Object = options.GetDataForEntry(clip.itemIndex); if (!item) continue;
                var row:Object = item.row; var layout:Object = clipLayout[clip];
                if (!layout) {
                    layout = {width:clip.Border_mc.width, height:clip.Border_mc.height, checkX:clip.CheckBox_mc.x};
                    clipLayout[clip] = layout;
                }
                clip.Border_mc.width = WIDTH; clip.Fill_mc.width = WIDTH;
                clip.CheckBox_mc.x = Number(layout.checkX) + WIDTH - Number(layout.width);
                var tf:TextField = clip.Text_mc.textField as TextField;
                if (tf) {
                    tf.autoSize = "none"; tf.multiline = false; tf.wordWrap = false;
                    tf.width = (row.header ? WIDTH - 25 : clip.CheckBox_mc.x - 20) - tf.x;
                    tf.height = CONFIG::largeText ? 42 : 36;
                    tf.defaultTextFormat = bodyFormat; tf.text = String(row.title); tf.setTextFormat(bodyFormat);
                    tf.textColor = row.header ? 0x9EA9B0 : clip.selected ? 0x101820 : 0xEDEDED;
                    Object(definition("Shared.GlobalFunc")).TruncateSingleLineText(tf);
                }
                var height:Number = Math.max(Number(layout.height), CONFIG::largeText ? 64 : 49);
                if (clip.Border_mc.height != height) { clip.Border_mc.height = height; needsLayout = true; }
                clip.Fill_mc.height = height;
            }
            if (needsLayout && !options.scrollbarScrolling) options.UpdateContainerRect();
        }
        private function gameFormat(font:String, size:Number, color:uint):TextFormat
        {
            var format:TextFormat = new TextFormat();
            format.font = font; format.size = size; format.color = color; return format;
        }
        private function field(text:String, x:Number, y:Number, width:Number, height:Number, format:TextFormat):TextField
        {
            var result:TextField = new TextField(); result.embedFonts = true;
            result.defaultTextFormat = format; result.text = text; result.setTextFormat(format); result.textColor = uint(format.color);
            result.x = x; result.y = y; result.width = width; result.height = height;
            result.selectable = false; result.mouseEnabled = false; return result;
        }
        private function html(text:String):String
        {
            return text.split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
        }
    }
}
