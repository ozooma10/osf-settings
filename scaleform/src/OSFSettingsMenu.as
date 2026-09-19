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
    import flash.text.TextFormat;
    import flash.ui.Keyboard;
    import flash.utils.Dictionary;
    import flash.utils.getDefinitionByName;
    import flash.utils.getTimer;

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
        private var issues:Array = [];
        private var rootPage:String = "mods";
        private var nextIssuePoll:int = 0;
        private var mods:Array = [];
        private var groups:Array = [];
        private var modID:String = "";
        private var groupID:String = "";
        private var options:Object;
        private var nativeHotkeys:NativeHotkeysList;
        private var keybindings:KeybindingsPage;
        private var stringEditor:StringSetting;
        private var stringConfirmHeld:Boolean;
        private var searchExitFrame:int = -10;
        private var navigationFrame:int = -1;
        private var bindingSelection:String = "";
        private var types:Class;
        private var bar:Object;
        private var background:MovieClip;
        private var menuStage:Stage;
        private var heading:TextField;
        private var section:TextField;
        private var count:TextField;
        private var detailTitle:TextField;
        private var detailLabel:TextField;
        private var detailDivider:Sprite = new Sprite();
        private var issueDetails:IssueDetails;
        private var changedLegend:Sprite = new Sprite();
        private var detailHint:TextField;
        private var defaultLabel:TextField;
        private var defaultValue:TextField;
        private var status:TextField;
        private var empty:TextField;
        private var tabs:Sprite = new Sprite();
        private var tabViewport:Sprite = new Sprite();
        private var drawnPages:Array = [];
        private var drawnGroupID:String = "";
        private var rowViews:Dictionary = new Dictionary(true);
        private var resetButton:Object;
        private var acceptButton:Object;
        private var backButton:Object;
        private var buttonData:Object = {};
        private var pageBar:Object;
        private var clearButton:Object;
        private var captureRow:Object;
        private var captureBinding:MovieClip;
        private var captureReady:Boolean = false;
        CONFIG::testHarness {
            include "../../tests/harness/MenuObservations.as";
        }

        public function OSFSettingsMenu()
        {
            addEventListener(Event.ADDED_TO_STAGE, added);
            addEventListener(Event.REMOVED_FROM_STAGE, removed);
        }
        private function definition(name:String):Class
        {
            if (!initialized) startupPhase = "resolve " + name;
            var result:Class;
            try { result = getDefinitionByName(name) as Class; }
            catch (error:Error) { throw new Error("Missing game class: " + name + ": " + error.toString()); }
            if (!result) throw new Error("Missing game class: " + name);
            return result;
        }
        private function create(name:String):Object
        {
            var c:Class = definition(name); var result:Object = new c();
            CONFIG::preview { BGSCodeObj.previewConstruct(name,result); }
            return result;
        }
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
                startupPhase = "populate settings"; readIssues(); refresh(false);
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
            startupPhase = "build authored menu text";
            var chrome:Sprite = new Sprite(); chrome.mouseEnabled = false; addChild(chrome);
            chrome.graphics.lineStyle(1, MenuStyle.LINE);
            chrome.graphics.moveTo(MenuStyle.LEFT, 258); chrome.graphics.lineTo(MenuStyle.RIGHT, 258);
            chrome.graphics.moveTo(MenuStyle.LEFT, 914); chrome.graphics.lineTo(MenuStyle.RIGHT, 914);
            chrome.graphics.lineStyle();
            detailDivider.mouseEnabled = false; addChild(detailDivider);
            detailDivider.graphics.lineStyle(1, MenuStyle.LINE);
            detailDivider.graphics.moveTo(1210, 656); detailDivider.graphics.lineTo(MenuStyle.RIGHT, 656);
            // Match SettingsPanel's 40px rail and roughly 47px stripe pitch.
            for (var i:int = 0; i < 14; ++i) {
                var y:Number = 274 + i * 47.25;
                chrome.graphics.beginFill(MenuStyle.WHITE); chrome.graphics.moveTo(60, y);
                chrome.graphics.lineTo(100, y + 40); chrome.graphics.lineTo(100, y + 62.3);
                chrome.graphics.lineTo(60, y + 22.3); chrome.graphics.endFill();
            }
            var rail:TextField = label("MOD SETTINGS", 0, 0, 250, 40, 25, 0xD3DDDF, true);
            rail.rotation = -90; rail.x = 60; rail.y = 268;
            heading = label("", MenuStyle.LEFT, 98, 1320, 85, CONFIG::largeText ? 60 : 52, MenuStyle.WHITE, true);
            tabViewport.x = MenuStyle.LEFT; tabViewport.y = 196;
            tabViewport.scrollRect = new Rectangle(0, 0, 1728, 64); tabViewport.addChild(tabs); addChild(tabViewport);
            section = label("", MenuStyle.LEFT, 305, 750, 45, CONFIG::largeText ? 30 : 27, MenuStyle.WHITE, true);
            count = label("", 902, 308, 230, 40, 23, MenuStyle.MUTED, true); alignRight(count);
            options = create("Shared.Components.SystemPanels.SettingsOptionList");
            configureList(options, "OptionListEntry");
            options.addEventListener("SettingsOptionEntry_ValueChanged", valueChanged);
            detailLabel = label("SELECTED SETTING", 1210, 363, 630, 36, 21, MenuStyle.MUTED, true);
            detailTitle = label("", 1210, 409, 634, 102, CONFIG::largeText ? 38 : 34);
            detailTitle.multiline = true; detailTitle.wordWrap = true;
            detailHint = label("", 1210, 486, 634, 160, CONFIG::largeText ? 30 : 27, MenuStyle.MUTED);
            detailHint.multiline = true; detailHint.wordWrap = true; detailHint.mouseEnabled = true;
            var descriptionFormat:TextFormat = detailHint.defaultTextFormat;
            descriptionFormat.leading = CONFIG::largeText ? 12 : 10; detailHint.defaultTextFormat = descriptionFormat;
            detailHint.addEventListener(MouseEvent.MOUSE_WHEEL, scrollDescription);
            defaultLabel = label("DEFAULT", 1210, 680, 420, 44, 23, MenuStyle.MUTED, true);
            defaultValue = label("", 1674, 680, 170, 44, 25, MenuStyle.WHITE, true); alignRight(defaultValue);
            issueDetails = new IssueDetails(); issueDetails.visible = false; addChild(issueDetails);
            status = label("Changes are saved automatically.", MenuStyle.LEFT, 938, 1180, 52, 21, MenuStyle.MUTED, true);
            var legend:TextField = label("Changed from default", 1450, 938, 394, 36, 21, MenuStyle.MUTED, true);
            changedLegend.mouseEnabled = false; changedLegend.mouseChildren = false;
            addChild(changedLegend); changedLegend.addChild(legend);
            alignRight(legend); MenuStyle.diamond(changedLegend.graphics, 1840 - legend.textWidth - 20, 954, MenuStyle.ACCENT);
            empty = label("", MenuStyle.LEFT + 20, MenuStyle.LIST_TOP + 22, 970, 130, MenuStyle.BODY_SIZE, MenuStyle.MUTED);
            empty.multiline = true; empty.wordWrap = true;
            pageBar = create("Shared.Components.ButtonControls.ButtonBar.ButtonBar");
            pageBar.x = MenuStyle.LEFT; pageBar.y = 1008; addChild(pageBar as MovieClip); pageBar.Initialize(0, 28);
            pageBar.scaleX = 1.1; pageBar.scaleY = 1.1;
            button("PREV PAGE", "LShoulder", function():void { changePage(-1); }, pageBar);
            button("NEXT PAGE", "RShoulder", function():void { changePage(1); }, pageBar);
            pageBar.RefreshButtons();
            bar = create("Shared.Components.ButtonControls.ButtonBar.ButtonBar");
            bar.x = MenuStyle.RIGHT; bar.y = 1008; addChild(bar as MovieClip); bar.Initialize(1, 38);
            bar.scaleX = 1.25; bar.scaleY = 1.25;
            acceptButton = button("TOGGLE", "Accept", accept);
            resetButton = button("RESET SETTING", "YButton", reset);
            clearButton = button("CLEAR BINDING", "XButton", clearBinding);
            backButton = button("ALL MODS", "Cancel", back); bar.RefreshButtons();
            captureBinding = create("Binding") as MovieClip;
            captureBinding.mouseEnabled = false; captureBinding.mouseChildren = false;
            captureBinding.visible = false;
            nativeHotkeys = new NativeHotkeysList(options, create, definition, BGSCodeObj, nativeBindingsChanged);
            keybindings = new KeybindingsPage(BGSCodeObj, nativeHotkeys, function():void { if (!refreshing) populate(true); }, focusResults);
            addChild(keybindings);
            stringEditor = new StringSetting(); addChild(stringEditor);
            options.addEventListener("SettingsControlListEnty_ActiveBindingChanged", selectionChanged);
            var popup:MovieClip = nativeHotkeys.popup as MovieClip;
            addChild(popup);
            var bounds:Rectangle = popup.getBounds(popup);
            popup.x = (1920 - bounds.width) / 2 - bounds.x;
            popup.y = (1080 - bounds.height) / 2 - bounds.y;
            menuStage.stageFocusRect = false;
            menuStage.addEventListener(Event.RESIZE, resizeBackground); resizeBackground();
            menuStage.addEventListener(Event.DEACTIVATE, focusLost);
            menuStage.addEventListener(KeyboardEvent.KEY_DOWN, keyDown, true, 50);
            menuStage.addEventListener(KeyboardEvent.KEY_UP, keyUp, true, 50);
            menuStage.addEventListener(MouseEvent.MOUSE_DOWN, mouseFocus, true);
            CONFIG::testHarness { menuStage.addEventListener(MouseEvent.CLICK, testClick, true); }
            addEventListener(Event.ENTER_FRAME, advance);
        }
        private function configureList(list:Object, entryClass:String):void
        {
            list.x = MenuStyle.LEFT; list.y = MenuStyle.LIST_TOP; addChild(list as MovieClip);
            var config:Object = create("Shared.AS3.BSScrollingConfigParams");
            config.EntryClassName = entryClass; config.VerticalSpacing = 4;
            config.TruncateToFit = true; config.RestoreIndex = true; config.WrapAround = false;
            list.Configure(config);
            list.Border_mc.x = 0; list.Border_mc.y = 0;
            list.Border_mc.width = MenuStyle.LIST_WIDTH; list.borderHeight = MenuStyle.LIST_HEIGHT;
            list.scrollBarHeight = MenuStyle.LIST_HEIGHT;
            var entries:DisplayObject = MovieClip(list).getChildByName("EntryHolder_mc");
            entries.x = 0; entries.y = 0;
            entries.scrollRect = new Rectangle(0, 0, MenuStyle.LIST_WIDTH, MenuStyle.LIST_HEIGHT);
            if (list.ScrollBar) list.ScrollBar.x = MenuStyle.LIST_WIDTH + 14;
            list.addEventListener("ScrollingEvent::selectionChange", selectionChanged);
            list.addEventListener("ScrollingEvent::itemPress", itemPressed);
            list.addEventListener("ScrollingEvent::playFocusSound", focusSound);
        }
        private function nativeBindingsChanged(message:String, refreshRows:Boolean):void
        {
            if (!initialized || closing) return;
            if (message) MenuStyle.setText(status, message);
            if (refreshRows) requestedRefresh = true;
            options.disableInput = bindingBusy() || Boolean(captureRow);
            options.disableSelection = bindingBusy();
            describe();
        }
        private function bindingBusy():Boolean { return nativeHotkeys && (nativeHotkeys.busy || nativeHotkeys.saving); }
        private function bindingsPage():Boolean { return !modID && rootPage == "bindings"; }
        private function searching():Boolean { return keybindings && keybindings.searching; }
        private function editingString():Boolean { return stringEditor && stringEditor.visible; }
        private function focusResults():void
        {
            searchExitFrame = frame;
            menuStage.focus = options as MovieClip;
            options.disableInput = bindingBusy();
        }
        private function issuesPage():Boolean { return !modID && rootPage == "issues"; }
        private function pages():Array
        {
            return modID ? groups : [{id:"mods", title:"ALL MODS"}, {id:"bindings", title:"KEYBINDINGS"}, {id:"issues", title:"MOD ISSUES" + (issues.length ? " (" + issues.length + ")" : "")}];
        }
        private function activePage():String { return modID ? groupID : rootPage; }
        private function selectPage(id:String):void
        {
            if (modID) groupID = id;
            else rootPage = id;
            if (bindingsPage()) keybindings.open(allRows);
            else keybindings.close();
            readIssues(); populate(); drawTabs();
        }
        private function button(text:String, eventName:String, callback:Function, target:Object = null):Object
        {
            var eventClass:Class = definition("Shared.Components.ButtonControls.ButtonData.UserEventData");
            var dataClass:Class = definition("Shared.Components.ButtonControls.ButtonData.ButtonBaseData");
            var factory:Class = definition("Shared.Components.ButtonControls.ButtonFactory.ButtonFactory");
            buttonData[eventName] = new dataClass(text, new eventClass(eventName, callback));
            return Object(factory).AddToButtonBar("BasicButton", buttonData[eventName], target || bar);
        }
        private function alignRight(field:TextField):void
        {
            var format:TextFormat = field.defaultTextFormat; format.align = "right";
            field.defaultTextFormat = format; field.setTextFormat(format);
        }
        private function readIssues():Boolean
        {
            nextIssuePoll = getTimer() + 1000;
            var latest:Array = BGSCodeObj.getIssues() as Array || [];
            var changed:Boolean = latest.length != issues.length;
            for (var i:int = 0; !changed && i < latest.length; ++i) {
                for each (var field:String in ["mod", "id", "modTitle", "title", "severity", "impact", "nextSteps"]) {
                    if (latest[i][field] != issues[i][field]) { changed = true; break; }
                }
            }
            if (changed) issues = latest;
            return changed;
        }
        private function refresh(preserve:Boolean = true):void
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
            nativeHotkeys.populate(allRows);
            nativeHotkeys.fullPage = bindingsPage();
            keybindings.visible = bindingsPage();
            var listHeight:Number = bindingsPage() ? 250 : MenuStyle.LIST_HEIGHT;
            options.y = bindingsPage() ? 634 : MenuStyle.LIST_TOP;
            options.borderHeight = listHeight; options.scrollBarHeight = listHeight;
            MovieClip(options).getChildByName("EntryHolder_mc").scrollRect = new Rectangle(0,0,MenuStyle.LIST_WIDTH,listHeight);
            section.visible = count.visible = !bindingsPage();
            empty.y = options.y + 22;
            var hasHotkeys:Boolean = false;
            var selected:int = preserve ? options.selectedIndex : 0;
            var scroll:int = preserve ? options.scrollPosition : 0;
            var selectedIssue:Object = preserve && (issuesPage() || bindingsPage()) ? current() : null;
            if (preserve && bindingsPage() && !selectedIssue && bindingSelection) selectedIssue = {identity:bindingSelection};
            var data:Array = []; var source:Array = bindingsPage() ? keybindings.filtered() : issuesPage() ? issues : modID ? allRows : mods;
            for each (var row:Object in source) {
                if (modID && (row.mod != modID || row.group != groupID)) continue;
                if (row.type == "hotkey") hasHotkeys = true;
                var slider:Boolean = modID != "" && NumericSetting.isSlider(row);
                // The vanilla entry multiplies fValue by 100. Our slider stores integer offsets.
                data.push({row:row, sText:html(String(row.title)), uID:data.length, bDisabled:false, bShowSpinner:false,
                    uCategory:0, bEnabled:!modID || row.editable, bSubSetting:false,
                    uType:!modID ? types.SDT_LINK : slider ? types.SDT_SLIDER : row.type == "enum" ? types.SDT_LARGE_STEPPER : row.type == "bool" ? types.SDT_CHECKBOX : types.SDT_LINK,
                    sliderData:{fValue:slider ? NumericSetting.position(row) / 100 : 0, sDisplayValue:NumericSetting.text(row, row.value)},
                    stepperData:{aStepperOptions:row.type == "enum" ? EnumSetting.labels(row) : [], uIndex:row.type == "enum" ? EnumSetting.index(row) : 0}, checkBoxData:{bChecked:row.type == "bool" && row.value}});
                if (selectedIssue && (bindingsPage() ? row.identity == selectedIssue.identity : row.mod == selectedIssue.mod && row.id == selectedIssue.id)) {
                    selected = data.length - 1;
                    scroll = Math.max(0, selected - (options.selectedIndex - options.scrollPosition));
                }
            }
            options.InitializeEntries(data);
            if (hasHotkeys) nativeHotkeys.open();
            options.selectedIndex = data.length ? Math.max(0, Math.min(selected, data.length - 1)) : -1;
            options.scrollPosition = Math.min(scroll, options.maxScrollPosition);
            options.disableInput = bindingBusy() || searching();
            if (!searching()) menuStage.focus = options as MovieClip;
            MenuStyle.setText(empty, data.length ? "" : bindingsPage() ? keybindings.emptyText : issuesPage() ? "No issues reported." : "No settings to display.");
            var title:String = "MOD SETTINGS"; var group:String = "ALL MODS";
            for each (var mod:Object in mods) if (mod.mod == modID) title = mod.title;
            for each (var page:Object in groups) if (page.id == groupID) group = page.title;
            MenuStyle.fit(heading, title.toUpperCase());
            MenuStyle.setText(section, issuesPage() ? "REPORTED ISSUES" : group.toUpperCase());
            MenuStyle.setText(count, data.length + (issuesPage() ? data.length == 1 ? " ISSUE" : " ISSUES" : modID ? data.length == 1 ? " SETTING" : " SETTINGS" : data.length == 1 ? " MOD" : " MODS"));
            if (!modID && (!bindingsPage() || !preserve)) {
                MenuStyle.setText(status, bindingsPage() ? "MainGameplay / PC   |   US ANSI   |   Enter and Num Enter share a native key code." : issuesPage() ? "Issues are reported by mods." : "Select a mod to view its settings.");
                status.textColor = MenuStyle.MUTED;
            } else if (!preserve) MenuStyle.setText(status, "Changes are saved automatically.");
            refreshing = false; describe(); decorate();
        }
        private function drawTabs():void
        {
            var pages:Array = this.pages();
            var active:String = activePage();
            var unchanged:Boolean = active == drawnGroupID && pages.length == drawnPages.length;
            for (var i:int = 0; unchanged && i < pages.length; ++i) {
                unchanged = pages[i].id == drawnPages[i].id && pages[i].title == drawnPages[i].title;
            }
            if (unchanged) return;
            drawnPages = pages; drawnGroupID = active;
            while (tabs.numChildren) tabs.removeChildAt(0);
            tabs.x = 0;
            var x:Number = 0; var activeX:Number = 0; var activeWidth:Number = 0;
            for each (var page:Object in pages) {
                var tab:Sprite = new Sprite(); tab.name = page.id; tab.x = x; tab.buttonMode = true;
                var text:TextField = MenuStyle.field(String(page.title).toUpperCase(), 0, 10, 440, 42,
                    CONFIG::largeText ? 29 : 25, page.id == active ? MenuStyle.WHITE : MenuStyle.MUTED, true);
                text.width = Math.min(440, text.textWidth + 8); MenuStyle.fit(text, String(page.title).toUpperCase());
                if (!modID && page.id == "issues" && issues.length && page.id != active) {
                    var countStart:int = text.text.lastIndexOf("(");
                    if (countStart >= 0) text.setTextFormat(new TextFormat(null, null, MenuStyle.ACCENT), countStart, text.length);
                }
                tab.graphics.beginFill(0, 0); tab.graphics.drawRect(0, 0, text.width + 34, 62); tab.graphics.endFill();
                if (page.id == active) {
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
            if (captureRow || bindingBusy() || searching() || editingString() || requestedRefresh || dragging()) return;
            selectPage(event.currentTarget.name);
        }
        private function changePage(direction:int):void
        {
            if (captureRow || bindingBusy() || searching() || editingString() || requestedRefresh || dragging()) return;
            var choices:Array = pages();
            if (choices.length < 2) return;
            for (var i:int = 0; i < choices.length; ++i) {
                if (choices[i].id == activePage()) {
                    selectPage(choices[(i + direction + choices.length) % choices.length].id); return;
                }
            }
        }
        private function current():Object { return options && options.selectedEntry ? options.selectedEntry.row : null; }
        private function describe():void
        {
            var row:Object = current();
            var reporting:Boolean = issuesPage();
            detailLabel.visible = detailTitle.visible = detailHint.visible = defaultLabel.visible = defaultValue.visible = detailDivider.visible = !reporting && !bindingsPage();
            MenuStyle.setText(detailLabel, modID ? "SELECTED SETTING" : "SELECTED MOD");
            changedLegend.visible = Boolean(modID);
            issueDetails.visible = reporting; issueDetails.show(reporting ? row : null);
            MenuStyle.setText(detailTitle, row ? row.title : "Nothing selected");
            detailHint.y = 409 + Math.max(68, detailTitle.textHeight + 20);
            detailHint.height = Math.max(64, 630 - detailHint.y);
            var hint:String = row ? String(row.hint || "") : "";
            MenuStyle.setText(detailHint, (row && row.requiresRestart ? "Changes take effect after restarting Starfield." + (hint ? "\n\n" : "") : "") + hint);
            detailHint.scrollV = 1;
            MenuStyle.setText(defaultLabel, modID ? "DEFAULT" : "SETTINGS");
            defaultValue.x = row && (row.type == "enum" || row.type == "key" || row.type == "string") ? 1434 : 1674;
            defaultValue.width = row && (row.type == "enum" || row.type == "key" || row.type == "string") ? 410 : 170;
            MenuStyle.fit(defaultValue, row && row.type == "hotkey" ? row.defaultName : row ? modID ? row.type == "enum" ? EnumSetting.text(row, row.defaultValue) :
                NumericSetting.text(row, row.defaultValue) : String(row.count) : "");
            resetButton.Visible = Boolean(!captureRow && !bindingBusy() && modID && row && row.editable && row.type != "hotkey");
            clearButton.Visible = Boolean(!captureRow && !bindingBusy() && (modID || bindingsPage()) && row &&
                (row.type == "key" && row.allowUnbound && Number(row.value) != 255 || row.type == "hotkey" && nativeHotkeys.canClear));
            buttonData.Accept.sButtonText = editingString() ? "SAVE" : row && row.type == "string" ? "EDIT TEXT" : captureRow ? "CONFIRM BINDING" : bindingsPage() ? "CHANGE BINDING" : !modID ? "OPEN" : row && (row.type == "key" || row.type == "hotkey") ? "CHANGE BINDING" : row && row.type == "enum" ? "NEXT CHOICE" : "TOGGLE";
            buttonData.Cancel.sButtonText = editingString() || captureRow || nativeHotkeys && nativeHotkeys.busy ? "CANCEL" : modID ? "ALL MODS" : "BACK";
            acceptButton.SetButtonData(buttonData.Accept); backButton.SetButtonData(buttonData.Cancel);
            acceptButton.Visible = !bindingBusy() && (captureRow ? captureReady : Boolean(row && (!modID || row.editable && (row.type == "bool" || row.type == "enum" || row.type == "key" || row.type == "hotkey" || row.type == "string"))));
            backButton.Visible = !bindingBusy();
            buttonData.YButton.sButtonText = reporting ? "SCROLL UP" : "RESET SETTING";
            buttonData.XButton.sButtonText = reporting ? "SCROLL DOWN" : "CLEAR BINDING";
            resetButton.SetButtonData(buttonData.YButton); clearButton.SetButtonData(buttonData.XButton);
            if (reporting) {
                acceptButton.Visible = false;
                resetButton.Visible = clearButton.Visible = issueDetails.scrollable;
            }
            if (bindingsPage()) {
                resetButton.Visible = false;
                if (row) bindingSelection = row.identity;
                keybindings.showSelection(row);
                acceptButton.Visible = Boolean(!bindingBusy() && !searching() && row && row.editable);
                clearButton.Visible = clearButton.Visible && !searching();
            }
            pageBar.visible = !captureRow && !bindingBusy() && !searching() && pages().length > 1;
            if (editingString()) { resetButton.Visible = clearButton.Visible = false; pageBar.visible = false; }
            bar.RefreshButtons();
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
            if (editingString()) { saveString(); return; }
            if (bindingBusy() || issuesPage() || searching() || frame <= searchExitFrame + 1) return;
            if (captureRow) { confirmBinding(); return; }
            if (closing || refreshing || requestedRefresh || activationFrame == frame || dragging()) return;
            activationFrame = frame;
            var row:Object = current(); if (!row) return;
            if (!modID && !bindingsPage()) { modID = row.mod; groupID = ""; refresh(false); }
            else if (row.type == "bool") options.OnEntryPressed();
            else if (row.type == "key" && row.editable) beginBinding(row);
            else if (row.type == "hotkey" && row.editable) nativeHotkeys.press();
            else if (row.type == "string" && row.editable) beginString(row);
            else if (row.type == "enum" && row.editable) {
                var clip:Object = options.FindClipForEntry(options.selectedIndex);
                if (clip) clip.LargeStepper_mc.PressHandler();
            }
        }
        private function reset():void
        {
            if (searching() || editingString()) return;
            if (issuesPage()) { issueDetails.scroll(-160); return; }
            if (captureRow || bindingBusy() || current() && current().type == "hotkey") return;
            var row:Object = current();
            if (!dragging() && modID && row && row.editable && row.value != row.defaultValue) edit(row, row.defaultValue);
        }
        private function valueChanged(event:Event):void
        {
            event.stopPropagation();
            if (refreshing || captureRow || bindingBusy() || editingString()) return;
            var data:Object = Object(event).params;
            var item:Object = options.GetDataForEntry(int(data.id));
            if (!modID || !item || !item.row.editable) return;
            var row:Object = item.row;
            if (row.type == "enum") {
                // The vanilla stepper reports a position; native storage owns the string value.
                var index:Number = Number(data.value);
                if (!requestedRefresh && isFinite(index) && index == Math.floor(index) && index >= 0 && index < row.options.length)
                    edit(row, row.options[int(index)].value);
            }
            else if (row.type == "bool") edit(row, Number(data.value) != 0);
            else if (NumericSetting.isSlider(row)) {
                // SettingsOptionListEntry reports BSSlider.value / 100.
                edit(row, NumericSetting.atPosition(row, Number(data.value) * 100));
            }
        }
        private function edit(row:Object, value:*):void
        {
            if (closing || refreshing || bindingBusy() || options.scrollbarScrolling || !row.editable) return;
            activationFrame = frame;
            if (value == row.value) { requestedRefresh = true; return; }
            var result:Object;
            if (row.type == "float") result = BGSCodeObj.setFloat(row.mod, row.key, Number(value));
            else if (row.type == "int") result = BGSCodeObj.setInt(row.mod, row.key, String(value));
            else if (row.type == "enum") result = BGSCodeObj.setEnum(row.mod, row.key, String(value));
            else if (row.type == "string") result = BGSCodeObj.setString(row.mod, row.key, String(value), Number(StringSetting.byteLength(String(value))));
            else if (row.type == "key") result = BGSCodeObj.setKey(row.mod, row.key, Number(value));
            else result = BGSCodeObj.setBool(row.mod, row.key, Boolean(value));
            if (result && result.ok) {
                if (row.type == "key") row.valueName = NumericSetting.text(row, value);
                row.value = value; describe();
            }
            MenuStyle.setText(status, result && result.ok ? "Changes are saved automatically." : result ? result.error : "Could not save this setting. Your previous value is unchanged.");
            status.textColor = result && result.ok ? MenuStyle.MUTED : MenuStyle.ACCENT;
            requestedRefresh = true;
        }
        private function beginString(row:Object):void
        {
            if (!BGSCodeObj.textInput(true)) return;
            stringConfirmHeld = false;
            options.disableInput = options.disableSelection = true;
            MovieClip(options).mouseEnabled = MovieClip(options).mouseChildren = false;
            tabs.mouseChildren = false;
            stringEditor.open(row); describe();
            MenuStyle.setText(status, "Edit the value, then save or cancel.");
            status.textColor = MenuStyle.MUTED;
        }
        private function saveString():void
        {
            if (!editingString() || frame <= activationFrame + 1) return;
            if (!stringEditor.valid) {
                stringEditor.showError("Use valid single-line text within " + stringEditor.row.maxLength + " UTF-8 bytes.");
                menuStage.focus = stringEditor.input; return;
            }
            var row:Object = stringEditor.row;
            var value:String = stringEditor.input.text;
            var result:Object = BGSCodeObj.setString(row.mod, row.key, value, Number(StringSetting.byteLength(value)));
            if (!result || !result.ok) {
                stringEditor.showError(result ? result.error : "Could not save. Your previous value is unchanged.");
                menuStage.focus = stringEditor.input; return;
            }
            finishString(false);
        }
        private function finishString(cancel:Boolean):void
        {
            stringEditor.close(); stringConfirmHeld = false;
            BGSCodeObj.textInput(false);
            options.disableSelection = false;
            MovieClip(options).mouseEnabled = MovieClip(options).mouseChildren = true;
            tabs.mouseChildren = true;
            searchExitFrame = activationFrame = frame;
            refresh();
            MenuStyle.setText(status, cancel ? "Text unchanged." : "Changes are saved automatically.");
            status.textColor = MenuStyle.MUTED;
        }
        private function back():void
        {
            if (editingString()) { finishString(true); return; }
            if (nativeHotkeys.busy) { nativeHotkeys.cancel(); return; }
            if (nativeHotkeys.saving) return;
            if (captureRow) { finishBinding(true); return; }
            if (frame <= searchExitFrame + 1) return;
            if (closing || dragging() || requestedRefresh) return;
            if (modID) { modID = ""; groupID = ""; rootPage = "mods"; readIssues(); refresh(false); return; }
            closing = true; options.disableInput = true; BGSCodeObj.close();
        }
        public function ProcessUserEvent(name:String, pressed:Boolean):Boolean
        {
            if (!initialized || closing) return false;
            if (editingString()) {
                if (name == "Cancel") { if (!pressed) finishString(true); return true; }
                return false; // Native raw keyboard/character events go to the field.
            }
            if (nativeHotkeys.popup.active) return Boolean(nativeHotkeys.popup.ProcessUserEvent(name, pressed));
            if (bindingBusy()) {
                if (pressed && name == "Cancel") nativeHotkeys.cancel();
                return true;
            }
            if (captureRow) {
                if (pressed && name == "Cancel") finishBinding(true);
                else if (pressed && name == "Accept") confirmBinding();
                return true;
            }
            // Returning false lets native raw keyboard/character forwarding
            // reach the focused field; no menu shortcut handlers run here.
            if (searching()) {
                if (name == "Cancel") { if (!pressed) back(); return true; }
                return false;
            }
            if (frame <= searchExitFrame + 1) return true;
            if (bindingsPage() && navigateBindings(name, pressed)) return true;
            if (bar.ProcessUserEvent(name, pressed)) return true;
            if (name == "LShoulder" || name == "RShoulder") {
                if (pageBar.visible) pageBar.ProcessUserEvent(name, pressed);
                return true;
            }
            var clip:Object = options.FindClipForEntry(options.selectedIndex);
            return clip && clip.IsSlider() ? Boolean(clip.Slider_mc.ProcessUserEvent(name, pressed)) : false;
        }
        private function mouseFocus(event:MouseEvent):void
        {
            CONFIG::testHarness { testMouseDown = testMouseEvent(event, testMouseDown); }
            if (captureRow || bindingBusy() || editingString()) return;
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
            if (editingString()) {
                if (event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.ESCAPE) {
                    if (event.keyCode == Keyboard.ENTER && frame > activationFrame + 1) stringConfirmHeld = true;
                    event.stopImmediatePropagation(); event.preventDefault();
                }
                return;
            }
            if (bindingBusy()) { event.stopImmediatePropagation(); event.preventDefault(); return; }
            if (captureRow) { event.stopImmediatePropagation(); event.preventDefault(); return; }
            if (keybindings && keybindings.searchKey(event)) return;
            if (!initialized || closing || refreshing || requestedRefresh || dragging()) return;
            if (bindingsPage() && (event.keyCode == Keyboard.UP || event.keyCode == Keyboard.DOWN || event.keyCode == Keyboard.LEFT || event.keyCode == Keyboard.RIGHT)) {
                navigateBindings(event.keyCode == Keyboard.UP ? "Up" : event.keyCode == Keyboard.DOWN ? "Down" : event.keyCode == Keyboard.LEFT ? "Left" : "Right", true);
                event.stopImmediatePropagation(); event.preventDefault(); return;
            }
            if (event.keyCode == Keyboard.B) reset();
            else if (event.keyCode == Keyboard.X && (issuesPage() || current() && (current().type == "key" || current().type == "hotkey"))) clearBinding();
            else if (issuesPage() && (event.keyCode == Keyboard.PAGE_UP || event.keyCode == Keyboard.PAGE_DOWN)) issueDetails.scroll(event.keyCode == Keyboard.PAGE_UP ? -360 : 360);
            else if (event.keyCode == 219) changePage(-1); // [ and ] also expose tabs without a mouse.
            else if (event.keyCode == 221) changePage(1);
            else if (event.keyCode == Keyboard.LEFT || event.keyCode == Keyboard.RIGHT) {
                var row:Object = current();
                if (modID && row && row.editable) {
                    if (row.type == "hotkey") nativeHotkeys.navigate(event);
                    else if (NumericSetting.isSlider(row)) {
                        var next:Number = NumericSetting.position(row) + (event.keyCode == Keyboard.RIGHT ? 1 : -1);
                        edit(row, NumericSetting.atPosition(row, next));
                    } else if (row.type == "enum") {
                        var clip:Object = options.FindClipForEntry(options.selectedIndex);
                        if (clip) clip.onKeyDownHandler(event);
                    } else if (row.type == "bool") edit(row, event.keyCode == Keyboard.RIGHT);
                }
            } else return;
            event.stopImmediatePropagation(); event.preventDefault();
        }
        private function keyUp(event:KeyboardEvent):void
        {
            if (editingString()) {
                if (event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.ESCAPE) {
                    event.stopImmediatePropagation(); event.preventDefault();
                    if (frame > activationFrame + 1) {
                        if (event.keyCode == Keyboard.ENTER) { if (stringConfirmHeld) saveString(); }
                        else finishString(true);
                    }
                    stringConfirmHeld = false;
                }
                return;
            }
            if (keybindings && keybindings.searchKey(event)) return;
            if (frame <= searchExitFrame + 1 || captureRow || bindingBusy() || event.keyCode == Keyboard.ENTER) event.stopImmediatePropagation();
        }
        private function navigateBindings(name:String, pressed:Boolean):Boolean
        {
            if (name != "Up" && name != "Down" && name != "Left" && name != "Right") return false;
            if (pressed && navigationFrame != frame) {
                navigationFrame = frame;
                if (name == "Up" || name == "Down") options.MoveSelection(name == "Up" ? -1 : 1);
                else nativeHotkeys.navigate(new KeyboardEvent(KeyboardEvent.KEY_DOWN,true,true,0,name == "Left" ? Keyboard.LEFT : Keyboard.RIGHT));
            }
            return true;
        }
        private function dragging():Boolean
        {
            if (!options) return false;
            if (options.scrollbarScrolling) return true;
            for (var i:int = 0; i < options.totalEntryClips; ++i) {
                var clip:Object = options.GetClipByIndex(i);
                if (clip && clip.IsSlider() && clip.Slider_mc.dragging) return true;
            }
            return false;
        }
        private function advance(event:Event):void
        {
            ++frame;
            if (initialized && !closing) {
                keybindings.advance(allRows,bindingBusy());
                options.disableInput = bindingBusy() || searching() || Boolean(captureRow) || editingString();
                if (captureRow) pollBinding();
                else if (!editingString()) {
                    if (requestedRefresh && !bindingBusy() && !dragging()) refresh();
                    if (getTimer() >= nextIssuePoll && !bindingBusy() && !dragging() && readIssues()) {
                        if (issuesPage()) populate(true);
                        drawTabs();
                    }
                    decorate();
                }
            }
            CONFIG::testHarness { advanceTestObservations(); }
        }
        private function clearBinding():void
        {
            if (searching() || editingString()) return;
            if (issuesPage()) { issueDetails.scroll(160); return; }
            if (current() && current().type == "hotkey") { nativeHotkeys.clearBinding(); return; }
            var row:Object = current();
            if (!captureRow && modID && row && row.type == "key" && row.allowUnbound) edit(row, 255);
        }
        private function beginBinding(row:Object):void
        {
            var result:Object = BGSCodeObj.beginKeyCapture(row.mod, row.key);
            if (!result || !result.ok) { MenuStyle.setText(status, "Could not start key capture."); return; }
            captureRow = row; captureRow.capturing = true; captureReady = false;
            CONFIG::testHarness { testCaptureState = "waiting"; }
            options.disableInput = true;
            MovieClip(options).mouseEnabled = false; MovieClip(options).mouseChildren = false;
            menuStage.focus = null;
            decorate();
            var clip:MovieClip = options.FindClipForEntry(options.selectedIndex) as MovieClip;
            clip.addChild(captureBinding);
            captureBinding.x = 720; captureBinding.y = (MenuStyle.ROW_HEIGHT - captureBinding.height) / 2;
            Object(captureBinding).SetBinding({aButtonName:[], aPCKeyName:[]});
            Object(captureBinding).SetState("listening"); captureBinding.visible = true;
            pageBar.visible = false;
            MenuStyle.setText(status, "Press a key. Escape cancels.");
            status.textColor = MenuStyle.MUTED;
            describe();
        }
        private function pollBinding():void
        {
            var state:Object = BGSCodeObj.pollKeyCapture();
            CONFIG::testHarness { testCaptureState = state ? state.state : "unavailable"; }
            if (!state || state.state == "cancelled" || state.state == "idle") { finishBinding(true); return; }
            if (state.state == "candidate" || state.state == "confirmed") {
                Object(captureBinding).SetBinding({aButtonName:[], aPCKeyName:[state.name]});
                if (!captureReady && state.released) {
                    captureReady = true; describe(); confirmBinding();
                } else if (state.state == "confirmed") confirmBinding();
            }
        }
        private function confirmBinding():void
        {
            if (!captureRow || !captureReady) return;
            var result:Object = BGSCodeObj.commitKeyCapture();
            if (result && result.ok) { finishBinding(false); return; }
            MenuStyle.setText(status, result && result.error ? result.error : "Could not save this setting. Your previous value is unchanged.");
            status.textColor = MenuStyle.ACCENT;
        }
        private function focusLost(event:Event):void
        {
            if (editingString()) finishString(true);
            nativeHotkeys.cancel();
            if (searching()) focusResults();
            if (captureRow) finishBinding(true);
        }
        public function onNativeBindingCancelled():void
        {
            if (nativeHotkeys) nativeHotkeys.cancel();
        }
        private function finishBinding(cancel:Boolean):void
        {
            if (cancel) BGSCodeObj.cancelKeyCapture();
            if (captureRow) captureRow.capturing = false;
            captureRow = null; captureReady = false; captureBinding.visible = false;
            if (captureBinding.parent) captureBinding.parent.removeChild(captureBinding);
            CONFIG::testHarness { testCaptureState = "idle"; }
            MovieClip(bar).visible = true;
            MovieClip(options).mouseEnabled = true; MovieClip(options).mouseChildren = true;
            activationFrame = frame;
            MenuStyle.setText(status, cancel ? "Binding unchanged." : "Changes are saved automatically.");
            status.textColor = MenuStyle.MUTED;
            refresh();
        }
        private function decorate():void
        {
            var needsLayout:Boolean = false;
            for (var i:int = 0; i < options.totalEntryClips; ++i) {
                var clip:MovieClip = options.GetClipByIndex(i) as MovieClip;
                if (!clip || Object(clip).itemIndex < 0) continue;
                var item:Object = options.GetDataForEntry(Object(clip).itemIndex); if (!item) continue;
                var view:Object = rowViews[clip];
                var issue:Boolean = item.row.type == "issue";
                if (!view || issue != (view is IssueRow)) {
                    if (view) clip.removeChild(view as DisplayObject);
                    view = issue ? new IssueRow() : new SettingsRow(); clip.addChild(view as DisplayObject); rowViews[clip] = view;
                }
                // Keep the vanilla hit area and behavior. Its authored timeline
                // colors must not recolor our text or selection bar.
                var slider:Object = Object(clip).Slider_mc;
                var showSlider:Boolean = modID != "" && NumericSetting.isSlider(item.row);
                var stepper:Object = Object(clip).LargeStepper_mc;
                var showStepper:Boolean = modID != "" && item.row.type == "enum" && item.row.editable;
                var binding:DisplayObject = modID || bindingsPage() ? nativeHotkeys.decorate(clip, item.row, Object(clip).itemIndex == options.selectedIndex) : null;
                clip.setChildIndex(view as DisplayObject, 0);
                for (var child:int = 0; child < clip.numChildren; ++child) {
                    var display:DisplayObject = clip.getChildAt(child);
                    display.visible = display == view || display == binding || (showSlider && display == slider) || (showStepper && display == stepper);
                }
                clip.transform.colorTransform = new ColorTransform(); clip.mouseChildren = showSlider || showStepper || binding != null;
                if (showSlider) {
                    slider.x = 540; slider.y = (MenuStyle.ROW_HEIGHT - slider.height) / 2; slider.width = 330;
                    slider.maxValue = NumericSetting.steps(item.row);
                    slider.disableRounding = false; slider.mouseWheelValueChange = 1;
                    if (!slider.dragging) slider.value = NumericSetting.position(item.row);
                    slider.transform.colorTransform = Object(clip).itemIndex == options.selectedIndex ?
                        new ColorTransform(0, 0, 0, 1, 8, 21, 28, 0) : new ColorTransform();
                }
                if (showStepper) {
                    // Keep vanilla arrows and hit areas; SettingsRow draws the label in our font.
                    stepper.textField.visible = false;
                    stepper.x = 540; stepper.width = 450;
                    stepper.y = (MenuStyle.ROW_HEIGHT - stepper.height) / 2;
                    stepper.transform.colorTransform = Object(clip).itemIndex == options.selectedIndex ?
                        new ColorTransform(0, 0, 0, 1, 8, 21, 28, 0) : new ColorTransform();
                }
                var border:MovieClip = Object(clip).Border_mc;
                border.x = 0; border.y = 0; border.width = MenuStyle.LIST_WIDTH;
                if (border.height != MenuStyle.ROW_HEIGHT) { border.height = MenuStyle.ROW_HEIGHT; needsLayout = true; }
                clip.x = 0; clip.y = (Object(clip).itemIndex - options.scrollPosition) * (MenuStyle.ROW_HEIGHT + 4);
                view.update(item.row, Object(clip).itemIndex == options.selectedIndex, modID == "" && !bindingsPage());
            }
            if (needsLayout && !dragging()) options.UpdateContainerRect();
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
            if (editingString()) { stringEditor.close(); BGSCodeObj.textInput(false); }
            if (nativeHotkeys) nativeHotkeys.dispose();
            if (captureRow) BGSCodeObj.cancelKeyCapture();
            if (menuStage) {
                menuStage.removeEventListener(Event.RESIZE, resizeBackground);
                menuStage.removeEventListener(Event.DEACTIVATE, focusLost);
                menuStage.removeEventListener(KeyboardEvent.KEY_DOWN, keyDown, true);
                menuStage.removeEventListener(KeyboardEvent.KEY_UP, keyUp, true);
                menuStage.removeEventListener(MouseEvent.MOUSE_DOWN, mouseFocus, true);
                CONFIG::testHarness { menuStage.removeEventListener(MouseEvent.CLICK, testClick, true); }
            }
            removeEventListener(Event.ENTER_FRAME, advance);
        }
        private function html(text:String):String
        {
            return text.split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
        }
    }
}
