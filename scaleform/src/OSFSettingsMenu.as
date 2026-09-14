package
{
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.display.Stage;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.ColorTransform;
    import flash.geom.Rectangle;
    import flash.text.TextField;
    import flash.text.TextFieldType;
    import flash.text.TextFormat;
    import flash.ui.Keyboard;
    import flash.utils.Dictionary;
    import flash.utils.getDefinitionByName;

    [SWF(width="1920", height="1080", frameRate="60", backgroundColor="#08151C")]
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
        private var allRows:Array = [];
        private var mods:Array = [];
        private var groups:Array = [];
        private var modID:String = "";
        private var groupID:String = "";
        private var query:String = "";
        private var options:Object;
        private var types:Class;
        private var bar:Object;
        private var background:MovieClip;
        private var menuStage:Stage;
        private var breadcrumb:TextField;
        private var heading:TextField;
        private var section:TextField;
        private var count:TextField;
        private var detailTitle:TextField;
        private var detailHint:TextField;
        private var defaultLabel:TextField;
        private var defaultValue:TextField;
        private var status:TextField;
        private var empty:TextField;
        private var searchInput:TextField;
        private var tabs:Sprite = new Sprite();
        private var tabViewport:Sprite = new Sprite();
        private var rowViews:Dictionary = new Dictionary(true);
        private var resetButton:Object;
        private var acceptButton:Object;
        private var backButton:Object;
        private var buttonData:Object = {};
        private var pageHint:TextField;

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
                startupPhase = "populate settings"; refresh(false, true);
                startupPhase = "ready"; BGSCodeObj.startup(startupPhase);
            } catch (error:Error) {
                initialized = false; closing = true;
                BGSCodeObj.startupFailed(startupPhase + ": " + error.toString());
            }
        }
        private function label(text:String, x:Number, y:Number, width:Number, height:Number,
                               size:Number, color:uint = 0xF1F2EC, condensed:Boolean = false):TextField
        {
            var result:TextField = MenuStyle.field(text, x, y, width, height, size, color, condensed);
            addChild(result); return result;
        }
        private function buildMenu():void
        {
            background = create("FullBackground") as MovieClip;
            background.mouseEnabled = false; background.mouseChildren = false; addChild(background);
            types = definition("Shared.Components.SystemPanels.SettingsOptionListEntry");
            var chrome:Sprite = new Sprite(); chrome.mouseEnabled = false; addChild(chrome);
            chrome.graphics.lineStyle(1, MenuStyle.LINE);
            chrome.graphics.moveTo(MenuStyle.LEFT, 258); chrome.graphics.lineTo(MenuStyle.RIGHT, 258);
            chrome.graphics.moveTo(MenuStyle.LEFT, 914); chrome.graphics.lineTo(MenuStyle.RIGHT, 914);
            chrome.graphics.moveTo(1210, 656); chrome.graphics.lineTo(MenuStyle.RIGHT, 656);
            chrome.graphics.lineStyle();
            for (var i:int = 0; i < 5; ++i) {
                var y:Number = 350 + i * 94;
                chrome.graphics.beginFill(0xC9D4D7); chrome.graphics.moveTo(40, y);
                chrome.graphics.lineTo(70, y + 30); chrome.graphics.lineTo(70, y + 51);
                chrome.graphics.lineTo(40, y + 21); chrome.graphics.endFill();
            }
            var rail:TextField = label("MOD SETTINGS", 0, 0, 250, 40, 25, 0xD3DDDF, true);
            rail.rotation = -90; rail.x = 40; rail.y = 268;
            breadcrumb = label("", MenuStyle.LEFT, 58, 1240, 35, 23, MenuStyle.MUTED, true);
            heading = label("", MenuStyle.LEFT, 98, 1320, 85, CONFIG::largeText ? 60 : 52, MenuStyle.WHITE, true);
            tabViewport.x = MenuStyle.LEFT; tabViewport.y = 196;
            tabViewport.scrollRect = new Rectangle(0, 0, 1728, 64); tabViewport.addChild(tabs); addChild(tabViewport);
            section = label("", MenuStyle.LEFT, 305, 750, 45, CONFIG::largeText ? 30 : 27, MenuStyle.WHITE, true);
            count = label("", 902, 308, 230, 40, 23, MenuStyle.MUTED, true); alignRight(count);
            options = create("Shared.Components.SystemPanels.SettingsOptionList");
            options.x = MenuStyle.LEFT; options.y = MenuStyle.LIST_TOP; addChild(options as MovieClip);
            var config:Object = create("Shared.AS3.BSScrollingConfigParams");
            config.EntryClassName = "OptionListEntry"; config.VerticalSpacing = 4;
            config.TruncateToFit = true; config.RestoreIndex = true; config.WrapAround = false;
            options.Configure(config);
            options.Border_mc.x = 0; options.Border_mc.y = 0;
            options.Border_mc.width = MenuStyle.LIST_WIDTH; options.borderHeight = MenuStyle.LIST_HEIGHT;
            options.scrollBarHeight = MenuStyle.LIST_HEIGHT;
            var entries:DisplayObject = MovieClip(options).getChildByName("EntryHolder_mc");
            entries.x = 0; entries.y = 0;
            entries.scrollRect = new Rectangle(0, 0, MenuStyle.LIST_WIDTH, MenuStyle.LIST_HEIGHT);
            if (options.ScrollBar) options.ScrollBar.x = MenuStyle.LIST_WIDTH + 14;
            options.addEventListener("ScrollingEvent::selectionChange", selectionChanged);
            options.addEventListener("ScrollingEvent::itemPress", itemPressed);
            options.addEventListener("ScrollingEvent::playFocusSound", focusSound);
            options.addEventListener("SettingsOptionEntry_ValueChanged", valueChanged);
            label("SELECTED SETTING", 1210, 363, 630, 36, 21, MenuStyle.MUTED, true);
            detailTitle = label("", 1210, 409, 634, 102, CONFIG::largeText ? 38 : 34);
            detailTitle.multiline = true; detailTitle.wordWrap = true;
            detailHint = label("", 1210, 486, 634, 160, CONFIG::largeText ? 30 : 27, MenuStyle.MUTED);
            detailHint.multiline = true; detailHint.wordWrap = true; detailHint.mouseEnabled = true;
            var descriptionFormat:TextFormat = detailHint.defaultTextFormat;
            descriptionFormat.leading = CONFIG::largeText ? 12 : 10; detailHint.defaultTextFormat = descriptionFormat;
            detailHint.addEventListener(MouseEvent.MOUSE_WHEEL, scrollDescription);
            defaultLabel = label("DEFAULT", 1210, 680, 420, 44, 23, MenuStyle.MUTED, true);
            defaultValue = label("", 1674, 680, 170, 44, 25, MenuStyle.WHITE, true); alignRight(defaultValue);
            status = label("Changes apply automatically.", MenuStyle.LEFT, 938, 1180, 52, 21, MenuStyle.MUTED, true);
            var legend:TextField = label("Changed from default", 1450, 938, 394, 36, 21, MenuStyle.MUTED, true);
            alignRight(legend); MenuStyle.diamond(chrome.graphics, 1840 - legend.textWidth - 20, 954, MenuStyle.ACCENT);
            empty = label("", MenuStyle.LEFT + 20, MenuStyle.LIST_TOP + 22, 970, 130, MenuStyle.BODY_SIZE, MenuStyle.MUTED);
            empty.multiline = true; empty.wordWrap = true;
            pageHint = label("[ / ]  CHANGE PAGE", MenuStyle.LEFT, 1005, 430, 38, 21, MenuStyle.MUTED, true);
            buildSearch();
            bar = create("Shared.Components.ButtonControls.ButtonBar.ButtonBar");
            bar.x = MenuStyle.RIGHT; bar.y = 1008; addChild(bar as MovieClip); bar.Initialize(1, 38);
            bar.scaleX = 1.25; bar.scaleY = 1.25;
            acceptButton = button("TOGGLE", "Accept", accept);
            resetButton = button("RESET SETTING", "YButton", reset);
            backButton = button("ALL MODS", "Cancel", back); bar.RefreshButtons();
            menuStage.stageFocusRect = false;
            menuStage.addEventListener(Event.RESIZE, resizeBackground); resizeBackground();
            menuStage.addEventListener(KeyboardEvent.KEY_DOWN, keyDown, true, 50);
            menuStage.addEventListener(KeyboardEvent.KEY_UP, keyUp, true, 50);
            menuStage.addEventListener(MouseEvent.MOUSE_DOWN, mouseFocus, true);
            addEventListener(Event.ENTER_FRAME, advance);
        }
        private function buildSearch():void
        {
            var search:Sprite = new Sprite(); search.x = 1390; search.y = 78; addChild(search);
            search.graphics.lineStyle(1.5, MenuStyle.MUTED); search.graphics.drawCircle(16, 20, 10);
            search.graphics.moveTo(23, 27); search.graphics.lineTo(30, 34); search.graphics.drawRect(414, 3, 38, 40);
            search.addChild(MenuStyle.field("F", 425, 9, 30, 35, 21, MenuStyle.MUTED, true));
            searchInput = MenuStyle.field("", 44, 5, 350, 38, 23, MenuStyle.MUTED, true);
            searchInput.type = TextFieldType.INPUT; searchInput.selectable = true; searchInput.mouseEnabled = true;
            searchInput.maxChars = 80; searchInput.addEventListener(Event.CHANGE, searchChanged); search.addChild(searchInput);
            var prompt:TextField = MenuStyle.field("SEARCH", 44, 5, 350, 38, 23, MenuStyle.MUTED, true);
            search.addChild(prompt);
            search.addEventListener(MouseEvent.CLICK, function(event:MouseEvent):void { menuStage.focus = searchInput; });
            search.addEventListener(Event.ENTER_FRAME, function(event:Event):void {
                prompt.visible = searchInput.text.length == 0 && menuStage.focus != searchInput;
            });
        }
        private function button(text:String, eventName:String, callback:Function):Object
        {
            var eventClass:Class = definition("Shared.Components.ButtonControls.ButtonData.UserEventData");
            var dataClass:Class = definition("Shared.Components.ButtonControls.ButtonData.ButtonBaseData");
            var factory:Class = definition("Shared.Components.ButtonControls.ButtonFactory.ButtonFactory");
            buttonData[eventName] = new dataClass(text, new eventClass(eventName, callback));
            return Object(factory).AddToButtonBar("BasicButton", buttonData[eventName], bar);
        }
        private function alignRight(field:TextField):void
        {
            var format:TextFormat = field.defaultTextFormat; format.align = "right";
            field.defaultTextFormat = format; field.setTextFormat(format);
        }
        private function refresh(preserve:Boolean = true, firstOpen:Boolean = false):void
        {
            allRows = BGSCodeObj.getRows() as Array || [];
            mods = []; var seen:Dictionary = new Dictionary();
            for each (var row:Object in allRows) {
                if (!seen[row.mod]) {
                    var mod:Object = {mod:row.mod, title:row.modTitle, hint:row.modDescription, count:0};
                    seen[row.mod] = mod; mods.push(mod);
                }
                seen[row.mod].count++;
            }
            if (firstOpen && mods.length == 1) modID = mods[0].mod;
            if (!seen[modID]) modID = "";
            groups = []; seen = new Dictionary();
            for each (row in allRows) {
                if (row.mod == modID && !seen[row.group]) {
                    seen[row.group] = true; groups.push({id:row.group, title:row.groupTitle});
                }
            }
            if (!seen[groupID]) groupID = groups.length ? groups[0].id : "";
            populate(preserve); drawTabs();
        }
        private function populate(preserve:Boolean = false):void
        {
            refreshing = true; requestedRefresh = false;
            var selected:int = preserve ? options.selectedIndex : 0;
            var scroll:int = preserve ? options.scrollPosition : 0;
            var data:Array = []; var source:Array = modID ? allRows : mods;
            for each (var row:Object in source) {
                if (modID && (row.mod != modID || (!query && row.group != groupID))) continue;
                if (query && (String(row.title) + " " + String(row.hint)).toLowerCase().indexOf(query) < 0) continue;
                data.push({row:row, sText:html(String(row.title)), uID:data.length, bDisabled:false, bShowSpinner:false,
                    uCategory:0, bEnabled:true, bSubSetting:false, uType:modID ? types.SDT_CHECKBOX : types.SDT_LINK,
                    sliderData:{fValue:0, sDisplayValue:""}, stepperData:{aStepperOptions:[], uIndex:0},
                    checkBoxData:{bChecked:Boolean(row.value)}});
            }
            options.InitializeEntries(data);
            options.selectedIndex = data.length ? Math.max(0, Math.min(selected, data.length - 1)) : -1;
            options.scrollPosition = Math.min(scroll, options.maxScrollPosition);
            options.disableInput = menuStage.focus == searchInput;
            if (!options.disableInput) menuStage.focus = options as MovieClip;
            empty.text = data.length ? "" : query ? "No results. Try another search." : "No settings to display.";
            var title:String = "MOD SETTINGS"; var group:String = "ALL MODS";
            for each (var mod:Object in mods) if (mod.mod == modID) title = mod.title;
            for each (var page:Object in groups) if (page.id == groupID) group = page.title;
            MenuStyle.fit(breadcrumb, modID ? "MOD SETTINGS   /   " + title.toUpperCase() : "MOD SETTINGS   /   ALL MODS");
            MenuStyle.fit(heading, title.toUpperCase());
            section.text = query ? "SEARCH RESULTS" : group.toUpperCase();
            count.text = data.length + (modID ? data.length == 1 ? " SETTING" : " SETTINGS" : data.length == 1 ? " MOD" : " MODS");
            pageHint.visible = modID != "" && groups.length > 1;
            refreshing = false; describe(); decorate();
        }
        private function drawTabs():void
        {
            while (tabs.numChildren) tabs.removeChildAt(0);
            tabs.x = 0;
            var pages:Array = modID ? groups : [{id:"", title:"ALL MODS"}];
            var x:Number = 0; var activeX:Number = 0; var activeWidth:Number = 0;
            for each (var page:Object in pages) {
                var tab:Sprite = new Sprite(); tab.name = page.id; tab.x = x; tab.buttonMode = true;
                var text:TextField = MenuStyle.field(String(page.title).toUpperCase(), 0, 10, 440, 42,
                    CONFIG::largeText ? 29 : 25, page.id == groupID && !query ? MenuStyle.WHITE : MenuStyle.MUTED, true);
                text.width = Math.min(440, text.textWidth + 8); MenuStyle.fit(text, String(page.title).toUpperCase());
                tab.graphics.beginFill(0, 0); tab.graphics.drawRect(0, 0, text.width + 34, 62); tab.graphics.endFill();
                if (page.id == groupID && !query) {
                    tab.graphics.lineStyle(3, MenuStyle.WHITE); tab.graphics.moveTo(0, 62); tab.graphics.lineTo(text.width, 62);
                    activeX = x; activeWidth = text.width;
                }
                tab.addChild(text); tab.addEventListener(MouseEvent.CLICK, tabClicked); tabs.addChild(tab);
                x += text.width + 42;
            }
            if (activeX + activeWidth > 1728) tabs.x = 1728 - activeX - activeWidth;
        }
        private function tabClicked(event:MouseEvent):void
        {
            if (requestedRefresh) return;
            groupID = event.currentTarget.name; clearSearch(); populate(); drawTabs();
        }
        private function changePage(direction:int):void
        {
            if (!modID || groups.length < 2 || requestedRefresh) return;
            for (var i:int = 0; i < groups.length; ++i) {
                if (groups[i].id == groupID) {
                    groupID = groups[(i + direction + groups.length) % groups.length].id;
                    clearSearch(); populate(); drawTabs(); return;
                }
            }
        }
        private function searchChanged(event:Event):void
        {
            query = searchInput.text.replace(/^\s+|\s+$/g, "").toLowerCase(); populate(); drawTabs();
        }
        private function clearSearch():void
        {
            searchInput.text = ""; query = ""; menuStage.focus = options as MovieClip;
        }
        private function current():Object { return options && options.selectedEntry ? options.selectedEntry.row : null; }
        private function describe():void
        {
            var row:Object = current();
            detailTitle.text = row ? row.title : "Nothing selected";
            detailHint.y = 409 + Math.max(68, detailTitle.textHeight + 20);
            detailHint.height = Math.max(64, 630 - detailHint.y);
            detailHint.text = row ? String(row.hint || "") : ""; detailHint.scrollV = 1;
            defaultLabel.text = modID ? "DEFAULT" : "SETTINGS";
            defaultValue.text = row ? modID ? row.defaultValue ? "ON" : "OFF" : String(row.count) : "";
            resetButton.Visible = Boolean(modID && row);
            buttonData.Accept.sButtonText = modID ? "TOGGLE" : "OPEN";
            buttonData.Cancel.sButtonText = modID ? "ALL MODS" : "BACK";
            acceptButton.SetButtonData(buttonData.Accept); backButton.SetButtonData(buttonData.Cancel);
            acceptButton.Visible = Boolean(row); bar.RefreshButtons();
        }
        private function scrollDescription(event:MouseEvent):void
        {
            detailHint.scrollV -= event.delta; event.stopPropagation();
        }
        private function selectionChanged(event:Event):void { if (!refreshing) describe(); }
        private function focusSound(event:Event):void { Object(definition("Shared.GlobalFunc")).PlayMenuSound("UIMenuGeneralFocus"); }
        private function itemPressed(event:Event):void { accept(); }
        private function accept():void
        {
            if (closing || refreshing || requestedRefresh || activationFrame == frame || options.scrollbarScrolling) return;
            activationFrame = frame;
            var row:Object = current(); if (!row) return;
            if (!modID) { modID = row.mod; groupID = ""; clearSearch(); refresh(false); }
            else options.OnEntryPressed();
        }
        private function reset():void
        {
            var row:Object = current();
            if (modID && row && Boolean(row.value) != Boolean(row.defaultValue)) edit(row, Boolean(row.defaultValue));
        }
        private function valueChanged(event:Event):void
        {
            event.stopPropagation();
            var data:Object = Object(event).params;
            var item:Object = options.GetDataForEntry(int(data.id));
            if (modID && item) edit(item.row, Number(data.value) != 0);
        }
        private function edit(row:Object, value:Boolean):void
        {
            if (closing || refreshing || requestedRefresh || options.scrollbarScrolling) return;
            activationFrame = frame;
            var result:Object = BGSCodeObj.setBool(row.mod, row.key, value);
            status.text = result && result.ok ? "Changes apply automatically." : result ? result.error : "Could not save this setting. Your previous value is unchanged.";
            status.textColor = result && result.ok ? MenuStyle.MUTED : MenuStyle.ACCENT;
            requestedRefresh = true;
        }
        private function back():void
        {
            if (closing || options.scrollbarScrolling || requestedRefresh) return;
            if (query || menuStage.focus == searchInput) { clearSearch(); populate(); drawTabs(); return; }
            if (modID) { modID = ""; groupID = ""; refresh(false); return; }
            closing = true; options.disableInput = true; BGSCodeObj.close();
        }
        public function ProcessUserEvent(name:String, pressed:Boolean):Boolean
        {
            if (!initialized || closing) return false;
            if (menuStage.focus == searchInput) {
                if (!pressed && name == "Cancel") back();
                return true;
            }
            return Boolean(bar.ProcessUserEvent(name, pressed));
        }
        private function mouseFocus(event:MouseEvent):void
        {
            if (!initialized || closing) return;
            var target:DisplayObject = event.target as DisplayObject;
            if (!target || !MovieClip(options).contains(target)) return;
            menuStage.focus = options as MovieClip; options.disableInput = false;
            while (target && target != options) {
                if ("itemIndex" in target) { options.selectedIndex = Object(target).itemIndex; break; }
                target = target.parent;
            }
        }
        private function keyDown(event:KeyboardEvent):void
        {
            if (!initialized || closing || refreshing || requestedRefresh || options.scrollbarScrolling) return;
            if (menuStage.focus == searchInput) {
                options.disableInput = true;
                if (event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.ESCAPE || event.keyCode == Keyboard.TAB) {
                    event.preventDefault(); event.stopImmediatePropagation();
                    if (event.keyCode == Keyboard.ESCAPE) { clearSearch(); populate(); drawTabs(); }
                    else { menuStage.focus = options as MovieClip; options.disableInput = false; }
                }
                return;
            }
            if (event.keyCode == Keyboard.F) { menuStage.focus = searchInput; options.disableInput = true; }
            else if (event.keyCode == Keyboard.B) reset();
            else if (event.keyCode == 219) changePage(-1); // [ and ] also expose tabs without a mouse.
            else if (event.keyCode == 221) changePage(1);
            else if (event.keyCode == Keyboard.LEFT || event.keyCode == Keyboard.RIGHT) {
                var row:Object = current(); if (modID && row) edit(row, event.keyCode == Keyboard.RIGHT);
            } else return;
            event.stopImmediatePropagation(); event.preventDefault();
        }
        private function keyUp(event:KeyboardEvent):void
        {
            if (event.keyCode == Keyboard.ENTER) event.stopImmediatePropagation();
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
                var clip:MovieClip = options.GetClipByIndex(i) as MovieClip;
                if (!clip || Object(clip).itemIndex < 0) continue;
                var item:Object = options.GetDataForEntry(Object(clip).itemIndex); if (!item) continue;
                var view:SettingsRow = rowViews[clip];
                if (!view) { view = new SettingsRow(); clip.addChild(view); rowViews[clip] = view; }
                // Keep the vanilla hit area and behavior. Its authored timeline
                // colors must not recolor our text or selection bar.
                for (var child:int = 0; child < clip.numChildren; ++child) {
                    var display:DisplayObject = clip.getChildAt(child); display.visible = display == view;
                }
                clip.transform.colorTransform = new ColorTransform(); clip.mouseChildren = false;
                var border:MovieClip = Object(clip).Border_mc;
                border.x = 0; border.y = 0; border.width = MenuStyle.LIST_WIDTH;
                if (border.height != MenuStyle.ROW_HEIGHT) { border.height = MenuStyle.ROW_HEIGHT; needsLayout = true; }
                clip.x = 0; clip.y = (Object(clip).itemIndex - options.scrollPosition) * (MenuStyle.ROW_HEIGHT + 4);
                view.update(item.row, Object(clip).itemIndex == options.selectedIndex, modID == "");
            }
            if (needsLayout && !options.scrollbarScrolling) options.UpdateContainerRect();
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
        private function html(text:String):String
        {
            return text.split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
        }
    }
}
