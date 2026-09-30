package
{
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.display.Sprite;
    import flash.display.Stage;
    import flash.events.Event;
    import flash.events.FocusEvent;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
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
        private static const LEFT_BRACKET:uint = 219;
        private static const RIGHT_BRACKET:uint = 221;
        // Row types Accept acts on; numbers step with Left and Right instead.
        private static const ACCEPT_TYPES:Object = {bool:true, "enum":true, key:true, hotkey:true, string:true, action:true};
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
        private var mods:Array = [];
        private var groups:Array = [];
        private var modID:String = "";
        // The open sidebar section's ID; empty while the whole mod is open.
        private var groupID:String = "";
        private var options:Object;
        private var settingsList:SettingsList;
        private var nativeHotkeys:NativeHotkeysList;
        private var keybindings:KeybindingsPage;
        private var stringEditor:StringSetting;
        private var actionConfirmation:ActionConfirmation;
        private var actionAcceptHeld:Boolean = false;
        private var revision:String = "";
        private var launcher:LauncherPage;
        private var launcherAcceptHeld:Boolean = false;
        // The launcher row whose destination is loading; Settings stays open until native reports.
        private var launching:Object = null;
        private var nextRevisionPoll:int = 0;
        private var stringConfirmHeld:Boolean;
        private var searchExitFrame:int = -10;
        private var navigationFrame:int = -1;
        private var types:Class;
        private var bar:Object;
        private var background:MovieClip;
        private var menuStage:Stage;
        private var nav:NavigationPane;
        // The sidebar entry that is open, as NavigationPane.key() spells it.
        private var navSelection:String = "page/mods";
        private var navAcceptHeld:Boolean = false;
        // Group ID to the sidebar section (tab) that holds it, for the open mod.
        private var groupTabs:Object = {};
        private var heading:TextField;
        private var headerSummary:TextField;
        private var section:TextField;
        private var count:TextField;
        private var homeMods:TextField;
        private var homeCount:TextField;
        private var headerCount:TextField;
        private var homeDetails:HomeDetails;
        private var homeEmpty:HomeEmpty;
        private var settingsDetails:SettingsDetails;
        private var issueDetails:IssueDetails;
        private var issueSummary:IssueSummary;
        private var status:TextField;
        private var empty:TextField;
        private var buttons:Object = {};
        private var footerText:String;
        private var footerWidth:Number = NaN;
        private var footerDirty:Boolean = false;
        // Keyboard and gamepad moves carry their direction past section headers.
        private var moveDirection:int = 0;
        private var moveFrame:int = -10;
        // The list entry under the pointer, lit whichever side has focus.
        private var hoverIndex:int = -1;
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
                Localization.initialize(BGSCodeObj.getLocalization ? BGSCodeObj.getLocalization() : null);
                menuStage = stage; buildMenu(); initialized = true;
                // The menu opens in the sidebar, on Home.
                startupPhase = "populate settings"; refresh(false); focusNav(true);
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
            MenuDecoration.rule(chrome.graphics, MenuStyle.LEFT, MenuStyle.RIGHT, MenuStyle.HEADER_LINE);
            MenuDecoration.rule(chrome.graphics, MenuStyle.LEFT, MenuStyle.RIGHT, MenuStyle.FOOTER_LINE);
            // The sidebar replaces tabs: pages, mods and the open mod's sections.
            nav = new NavigationPane(navClicked); addChild(nav);
            // One header line: the open page's title, with a mod's summary at the right.
            heading = label("", MenuStyle.LEFT, MenuStyle.TITLE_TOP, MenuStyle.RIGHT - MenuStyle.LEFT - 460, MenuStyle.TITLE_SIZE + 16, MenuStyle.TITLE_SIZE, MenuStyle.WHITE, true);
            headerSummary = label("", MenuStyle.RIGHT - 440, MenuStyle.TITLE_TOP + MenuStyle.TITLE_SIZE - MenuStyle.SECTION_SIZE - 2, 440,
                MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            alignRight(headerSummary);
            section = label("", MenuStyle.LEFT, MenuStyle.SECTION_TOP, 750, MenuStyle.SECTION_SIZE + 14, MenuStyle.SECTION_SIZE, MenuStyle.WHITE, true);
            count = label("", MenuStyle.LEFT + MenuStyle.LIST_WIDTH - 230, MenuStyle.SECTION_TOP + 3, 230, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            alignRight(count);
            // Home headings carry their count right after the title: INTERFACES 10, MODS 3.
            headerCount = label("", MenuStyle.LEFT, MenuStyle.SECTION_TOP + 3, 200, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            homeMods = label(tr("home.mods"), MenuStyle.LEFT, 0, 750, MenuStyle.SECTION_SIZE + 14, MenuStyle.SECTION_SIZE, MenuStyle.WHITE, true);
            homeCount = label("", MenuStyle.LEFT, 0, 200, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            homeMods.visible = homeCount.visible = headerCount.visible = false;
            options = create("Shared.Components.SystemPanels.SettingsOptionList");
            settingsList = new SettingsList(options);
            configureList(options, "OptionListEntry");
            options.addEventListener("SettingsOptionEntry_ValueChanged", valueChanged);
            // Restore Home's list before vanilla handles a row's hover. Selection
            // events cannot do this while the interface shelf has disabled the list.
            options.addEventListener(MouseEvent.MOUSE_OVER, mouseFocus, true);
            options.addEventListener(MouseEvent.MOUSE_OVER, hoverEntry);
            options.addEventListener(MouseEvent.MOUSE_OUT, hoverEntry);
            options.addEventListener(MouseEvent.MOUSE_WHEEL, wheelList);
            options.addEventListener(MouseEvent.CLICK, clickStepper, true);
            settingsDetails = new SettingsDetails(); addChild(settingsDetails);
            issueDetails = new IssueDetails(); issueDetails.visible = false; addChild(issueDetails);
            issueSummary = new IssueSummary(); issueSummary.visible = false; addChild(issueSummary);
            homeDetails = new HomeDetails(); homeDetails.visible = false; addChild(homeDetails);
            homeEmpty = new HomeEmpty(function():void {
                if (!captureRow && !bindingBusy() && !requestedRefresh) openEntry({kind:"page", id:"bindings"});
            });
            addChild(homeEmpty);
            // Large text reserves a status line above the wider native button hints.
            status = label(tr("menu.autoSave"), MenuStyle.LEFT, 0, 900, 52, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            status.multiline = true; status.wordWrap = true;
            empty = label("", MenuStyle.LEFT + 16, MenuStyle.LIST_TOP + 16, MenuStyle.LIST_WIDTH - 40, 130, MenuStyle.BODY_SIZE, MenuStyle.MUTED);
            empty.multiline = true; empty.wordWrap = true;
            bar = create("Shared.Components.ButtonControls.ButtonBar.ButtonBar");
            // Native button scale; the _LRG movie's vanilla buttons are already larger.
            bar.x = MenuStyle.RIGHT; bar.y = MenuStyle.FOOTER_Y; addChild(bar as MovieClip); bar.Initialize(1, 30);
            button(tr("buttons.previousPage"), "LShoulder", function():void { changePage(-1); });
            button(tr("buttons.nextPage"), "RShoulder", function():void { changePage(1); });
            button(tr("buttons.toggle"), "Accept", accept);
            button(tr("buttons.reset"), "YButton", reset);
            button(tr("buttons.clearBinding"), "XButton", clearBinding);
            button(tr("menu.home"), "Cancel", back); bar.RefreshButtons();
            captureBinding = create("Binding") as MovieClip;
            captureBinding.mouseEnabled = false; captureBinding.mouseChildren = false;
            captureBinding.visible = false;
            nativeHotkeys = new NativeHotkeysList(options, create, definition, BGSCodeObj, nativeBindingsChanged);
            startupPhase = "build keybindings";
            keybindings = new KeybindingsPage(BGSCodeObj, nativeHotkeys, function():void { if (!refreshing) populate(true); }, focusResults);
            addChild(keybindings);
            keybindings.search.addEventListener(FocusEvent.FOCUS_IN, searchFocusChanged);
            keybindings.search.addEventListener(FocusEvent.FOCUS_OUT, searchFocusChanged);
            startupPhase = "build Home launchers";
            // Hovering the shelf moves focus between it and the list, but never out of the sidebar.
            launcher = new LauncherPage(function():void { if (!refreshing && !nav.focused) focusLauncher(true); }, accept,
                function():void { populate(true); focusLauncher(true); });
            launcher.visible = false; addChild(launcher);
            startupPhase = "build string editor";
            stringEditor = new StringSetting(); addChild(stringEditor);
            options.addEventListener("SettingsControlListEnty_ActiveBindingChanged", selectionChanged);
            var popup:MovieClip = nativeHotkeys.popup as MovieClip;
            addChild(popup);
            var bounds:Rectangle = popup.getBounds(popup);
            popup.x = (1920 - bounds.width) / 2 - bounds.x;
            popup.y = (1080 - bounds.height) / 2 - bounds.y;
            startupPhase = "build action confirmation";
            actionConfirmation = new ActionConfirmation(finishActionConfirmation); addChild(actionConfirmation);
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
            config.EntryClassName = entryClass; config.VerticalSpacing = MenuStyle.ROW_GAP;
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
            // The bindings page publishes its rows when the numeric snapshot is ready.
            if (refreshRows && !bindingsPage()) requestedRefresh = true;
            updateSelection();
        }
        private function bindingBusy():Boolean { return nativeHotkeys && (nativeHotkeys.busy || nativeHotkeys.saving); }
        private function bindingsPage():Boolean { return !modID && rootPage == "bindings"; }
        private function homePage():Boolean { return !modID && rootPage == "mods"; }
        private function launcherPage():Boolean { return homePage() && launcher && launcher.hasEntries && launcher.focused; }
        private function homeEmptyState():Boolean { return homePage() && !mods.length && launcher && !launcher.hasEntries; }
        private function expandedLauncher():Boolean { return homePage() && launcher && launcher.expanded; }
        private function focusLauncher(value:Boolean):void
        {
            launcher.focused = value;
            if (value) nav.focused = false;
            launcherAcceptHeld = false;
            syncInput();
            menuStage.focus = value ? launcher : options as MovieClip;
            if (homePage()) setStatus("");
            decorate(); updateSelection();
        }
        private function searching():Boolean { return keybindings && keybindings.searching; }
        private function editingString():Boolean { return stringEditor && stringEditor.visible; }
        private function confirmingAction():Boolean { return actionConfirmation && actionConfirmation.visible; }
        // A key capture, native rebind, text editor or confirmation owns input until it finishes.
        private function modalBusy():Boolean { return Boolean(captureRow) || bindingBusy() || editingString() || confirmingAction(); }
        // Changing page also waits for search, a pending refresh and any drag.
        private function pageLocked():Boolean { return modalBusy() || searching() || requestedRefresh || settingsList.dragging; }
        private function focusResults():void
        {
            // Leaving the search field lands in the results, never back in the sidebar.
            nav.focused = false;
            searchExitFrame = frame;
            menuStage.focus = options as MovieClip;
            syncInput();
        }
        private function searchFocusChanged(event:FocusEvent):void
        {
            footerDirty = true; // Apply after the focus transition has finished.
        }
        private function issuesPage():Boolean { return !modID && rootPage == "issues"; }
        // Sidebar entries: pinned pages, a heading, every mod, and the open mod's sections.
        private function navItems():Array
        {
            // Pinned pages keep their old tab names.
            var items:Array = [{kind:"page", id:"mods", name:"mods", title:tr("menu.home")}, {kind:"page", id:"bindings", name:"bindings", title:tr("menu.keybindings")},
                {kind:"page", id:"issues", name:"issues", title:tr("menu.issues"), count:issues.length || "", accent:issues.length > 0}];
            if (mods.length) items.push({kind:"heading", id:"mods", title:tr("home.mods"), count:mods.length});
            for each (var mod:Object in mods) {
                items.push({kind:"mod", id:mod.mod, name:"mod/" + mod.mod, title:mod.title, count:mod.count});
                if (mod.mod == modID && groups.length > 1)
                    for each (var tab:Object in groups)
                        items.push({kind:"section", id:mod.mod + "/" + tab.id, name:tab.id, mod:mod.mod, tab:tab.id, title:tab.title, count:tab.size});
            }
            return items;
        }
        private function updateNav():void
        {
            nav.update(navItems(), navSelection);
            // A page that disappeared (an uninstalled mod) falls back to whatever the cursor landed on.
            if (NavigationPane.key(nav.selected) != navSelection && nav.selected && nav.selected.kind != "section") {
                navSelection = NavigationPane.key(nav.selected);
            }
        }
        // Opening an entry changes the page at once; focus stays where it is.
        private function openEntry(item:Object):void
        {
            if (!item || item.kind == "heading") return;
            launcherAcceptHeld = false;
            var mod:String = item.kind == "page" ? "" : item.kind == "mod" ? item.id : item.mod;
            // A mod lists all its settings; one of its sections lists only its own.
            var tab:String = item.kind == "section" ? item.tab : "";
            // Pages reload whenever they are chosen, as their tabs did.
            var reload:Boolean = mod != modID || tab != groupID || item.kind == "page";
            if (item.kind == "page") rootPage = item.id;
            if (mod != modID) { modID = mod; groupID = ""; buildTabs(); }
            groupID = tab;
            if (bindingsPage()) { if (reload) keybindings.open(allRows); }
            else keybindings.close();
            navSelection = NavigationPane.key(item);
            updateNav();
            if (reload) populate();
            else { decorate(); updateSelection(); }
        }
        private function navClicked(item:Object):void
        {
            if (pageLocked()) return;
            openEntry(item); focusNav(true);
        }
        // The sidebar and the page share input: one of them has focus.
        private function focusNav(value:Boolean):void
        {
            // Repaint only when focus actually moves; a press on the sidebar must not redraw it.
            if (value == nav.focused) { if (value) menuStage.focus = nav; return; }
            nav.focused = value; navAcceptHeld = false;
            if (value) { launcher.focused = false; launcherAcceptHeld = false; }
            else if (homePage() && launcher.hasEntries && (!options.entryCount || expandedLauncher())) launcher.focused = true;
            syncInput();
            menuStage.focus = value ? nav : launcherPage() ? launcher : options as MovieClip;
            decorate(); updateSelection();
        }
        private function tabOf(row:Object):String { return groupTabs[row.group] ? groupTabs[row.group].id : row.group; }
        // Whether a row belongs on the open mod's page, narrowed to the open section if there is one.
        private function inPage(row:Object):Boolean
        {
            return modID != "" && row.mod == modID && row.type != "launcher" && (!groupID || tabOf(row) == groupID);
        }
        // Sidebar input: Up/Down open entries as they pass, Accept or Right enters the page.
        private function navigate(name:String, pressed:Boolean):Boolean
        {
            if (name == "Up" || name == "Down") {
                if (pressed && navigationFrame != frame) { navigationFrame = frame; openEntry(nav.move(name == "Up" ? -1 : 1)); }
                return true;
            }
            if (name == "Right") { if (pressed) enterPage(); return true; }
            if (name == "Accept") {
                // Enter on release, like the interface shelf, so the press cannot also act on the page.
                if (pressed) navAcceptHeld = true;
                else if (navAcceptHeld) { navAcceptHeld = false; enterPage(); }
                return true;
            }
            if (name == "Cancel") { if (!pressed) back(); return true; }
            // Anything else stays unhandled: a mouse button arrives as a disabled named event, and
            // only when the movie declines it does the engine forward the real Scaleform click.
            return false;
        }
        private function enterPage():void
        {
            if (homeEmptyState()) { openEntry({kind:"page", id:"bindings"}); return; }
            if (options.entryCount || homePage() && launcher.hasEntries) focusNav(false);
        }
        // Keybinding and issue rows show a second line; everything else is one line.
        private function rowHeight():Number { return issuesPage() ? IssueStyle.ROW_HEIGHT : bindingsPage() ? MenuStyle.TALL_ROW_HEIGHT : MenuStyle.ROW_HEIGHT; }
        // The nearest non-header entry from index, trying step first and then fallback.
        private function settingFrom(index:int, step:int, fallback:int):int
        {
            for each (var direction:int in [step, fallback]) {
                for (var i:int = index + direction; direction && i >= 0 && i < options.entryCount; i += direction)
                    if (options.GetDataForEntry(i).row.type != "section") return i;
            }
            return -1;
        }
        private function noteMove(name:String):void
        {
            var direction:int = name == "Up" || name == "PageUp" ? -1 : name == "Down" || name == "PageDown" ? 1 : 0;
            if (direction) { moveDirection = direction; moveFrame = frame; }
        }
        // Vanilla selection can land on a header; carry keyboard and gamepad moves past it.
        private function skipSection():Boolean
        {
            var index:int = options.selectedIndex;
            if (index < 0 || index >= options.entryCount || options.GetDataForEntry(index).row.type != "section") return false;
            var step:int = frame - moveFrame <= 1 && moveDirection ? moveDirection : 1;
            var target:int = settingFrom(index, step, -step);
            if (target >= 0 && target != index) options.selectedIndex = target;
            return true;
        }
        private function button(text:String, eventName:String, callback:Function):void
        {
            var eventClass:Class = definition("Shared.Components.ButtonControls.ButtonData.UserEventData");
            var dataClass:Class = definition("Shared.Components.ButtonControls.ButtonData.ButtonBaseData");
            var factory:Class = definition("Shared.Components.ButtonControls.ButtonFactory.ButtonFactory");
            var data:Object = new dataClass(text, new eventClass(eventName, callback));
            buttons[eventName] = {data:data, clip:Object(factory).AddToButtonBar("BasicButton", data, bar)};
        }
        private function updateButton(name:String, text:String, visible:Boolean):Boolean
        {
            var button:Object = buttons[name];
            var changed:Boolean = button.clip.Visible != visible || button.data.sButtonText != text;
            if (button.data.sButtonText != text) {
                button.data.sButtonText = text; button.clip.SetButtonData(button.data);
            }
            if (button.clip.Visible != visible) button.clip.Visible = visible;
            return changed;
        }
        // Errors draw in the accent color; everything else is muted.
        private function setStatus(text:String, error:Boolean = false):void
        {
            MenuStyle.setText(status, text);
            status.textColor = error ? MenuStyle.ACCENT : MenuStyle.MUTED;
        }
        private function alignRight(field:TextField):void
        {
            var format:TextFormat = field.defaultTextFormat; format.align = "right";
            field.defaultTextFormat = format; field.setTextFormat(format);
        }
        private function refresh(preserve:Boolean = true):void
        {
            // Read the revision first so a concurrent completion triggers another refresh.
            revision = String(BGSCodeObj.revision());
            issues = BGSCodeObj.getIssues() as Array || [];
            allRows = BGSCodeObj.getRows() as Array || [];
            mods = []; var seen:Dictionary = new Dictionary();
            for each (var row:Object in allRows) {
                if (row.type == "launcher") continue;
                if (!seen[row.mod]) {
                    var mod:Object = {mod:row.mod, title:row.modTitle, hint:row.modDescription, count:0, settings:0, actions:0, interfaces:0, hotkeys:[]};
                    seen[row.mod] = mod; mods.push(mod);
                }
                mod = seen[row.mod]; mod.count++;
                if (row.type == "hotkey") mod.hotkeys.push(row);
                else if (row.type == "action") mod.actions++;
                else mod.settings++;
            }
            for each (row in allRows) if (row.type == "launcher" && seen[row.mod]) seen[row.mod].interfaces++;
            // OSF Settings' only entry is the key that opens this menu, which Keybindings already lists.
            mods = mods.filter(function(item:Object, index:int, source:Array):Boolean {
                return item.mod != "osfsettings" || item.settings > 0 || item.actions > 0;
            });
            for each (mod in mods) { mod.summary = summaryParts(mod).join("  |  "); mod.chips = summaryParts(mod, false); }
            if (!seen[modID]) modID = "";
            buildTabs();
            populate(preserve); updateNav();
        }
        // Groups titled "Tab - Section" share a sidebar section; others are sections of their
        // own. A section keeps its first group's ID so unfolded groups keep theirs.
        private function buildTabs():void
        {
            groups = []; groupTabs = {}; var seen:Dictionary = new Dictionary(); var folded:Object = {};
            for each (var row:Object in allRows) {
                if (row.type == "launcher" || row.mod != modID) continue;
                if (seen[row.group]) { ++groupTabs[row.group].size; continue; }
                seen[row.group] = true;
                var title:String = String(row.groupTitle); var split:int = title.indexOf(" - ");
                var name:String = split > 0 ? title.substr(0, split) : title;
                var tab:Object = folded.hasOwnProperty(name) ? folded[name] : null;
                if (!tab) { tab = folded[name] = {id:row.group, title:name, size:0}; groups.push(tab); }
                groupTabs[row.group] = tab; ++tab.size;
            }
            // A section that is gone (or no longer has siblings) falls back to the whole mod.
            var found:Boolean = false;
            for each (tab in groups) if (tab.id == groupID && groups.length > 1) found = true;
            if (!found) {
                groupID = "";
                if (modID && navSelection.indexOf("section/") == 0) navSelection = "mod/" + modID;
            }
        }
        // The detail card lists hotkeys themselves, so its chips leave out their count.
        private function summaryParts(mod:Object, hotkeys:Boolean = true):Array
        {
            var parts:Array = [];
            if (mod.settings) parts.push(mod.settings == 1 ? tr("summary.setting") : tr("summary.settings", {count:mod.settings}));
            if (hotkeys && mod.hotkeys.length) parts.push(mod.hotkeys.length == 1 ? tr("summary.hotkey") : tr("summary.hotkeys", {count:mod.hotkeys.length}));
            if (mod.actions) parts.push(mod.actions == 1 ? tr("summary.action") : tr("summary.actions", {count:mod.actions}));
            if (mod.interfaces) parts.push(mod.interfaces == 1 ? tr("summary.interface") : tr("summary.interfaces", {count:mod.interfaces}));
            return parts;
        }
        private function populate(preserve:Boolean = false):void
        {
            refreshing = true; requestedRefresh = false;
            nativeHotkeys.populate(allRows);
            nativeHotkeys.fullPage = bindingsPage();
            keybindings.visible = bindingsPage();
            // With no mod settings, interfaces fill Home as a grid that cannot collapse.
            if (homePage()) launcher.populate(allRows, preserve, !mods.length);
            launcher.visible = homePage() && launcher.hasEntries;
            launcher.mouseEnabled = launcher.mouseChildren = launcher.visible;
            launcher.y = expandedLauncher() ? LauncherPage.GRID_TOP : MenuStyle.LIST_TOP;
            MovieClip(options).visible = !expandedLauncher() && !homeEmptyState();
            homeEmpty.visible = homeEmptyState();
            homeMods.visible = homeCount.visible = launcher.visible && !expandedLauncher();
            homeMods.y = launcher.y + launcher.shelfHeight + 18;
            homeCount.y = homeMods.y + 3;
            syncInput();
            // Mod pages start the list under the header; section headers name its parts.
            options.y = issuesPage() ? IssueStyle.TOP : bindingsPage() ? keybindings.listTop : homeMods.visible ? homeMods.y + MenuStyle.SECTION_SIZE + 20 :
                modID ? MenuStyle.SECTION_TOP : MenuStyle.LIST_TOP;
            // Whole rows only, so the last visible row is never cut by the footer.
            var pitch:Number = rowHeight() + MenuStyle.ROW_GAP;
            var listHeight:Number = Math.max(1, Math.floor((MenuStyle.LIST_BOTTOM - options.y + MenuStyle.ROW_GAP) / pitch)) * pitch - MenuStyle.ROW_GAP;
            var listWidth:Number = issuesPage() ? IssueStyle.LIST_WIDTH : MenuStyle.LIST_WIDTH;
            // Setting borderHeight makes vanilla update every clip, even for the same value.
            if (options.borderHeight != listHeight || options.Border_mc.width != listWidth) {
                options.Border_mc.width = listWidth; options.borderHeight = listHeight;
            }
            options.scrollBarHeight = listHeight;
            if (options.ScrollBar) options.ScrollBar.x = listWidth + 14;
            MovieClip(options).getChildByName("EntryHolder_mc").scrollRect = new Rectangle(0,0,listWidth,listHeight);
            empty.width = listWidth - 40;
            section.visible = !bindingsPage() && !issuesPage() && !homeEmptyState() && !modID;
            count.visible = issuesPage();
            count.x = MenuStyle.RIGHT - count.width;
            issueSummary.visible = issuesPage();
            if (issueSummary.visible) issueSummary.update(issues);
            headerCount.visible = section.visible && homePage();
            empty.y = options.y + 16;
            var hasHotkeys:Boolean = false;
            var selected:int = preserve ? options.selectedIndex : 0;
            var scroll:int = preserve ? options.scrollPosition : 0;
            var selectedIssue:Object = preserve && (issuesPage() || bindingsPage()) ? current() : null;
            var data:Array = []; var source:Array = bindingsPage() ? keybindings.filtered() : issuesPage() ? issues : modID ? allRows : mods;
            // A page with more than one group heads each group; a single group needs no header.
            var headed:Boolean = false, lastGroup:String = null, firstGroup:String = null;
            for each (row in allRows) if (inPage(row)) {
                if (firstGroup == null) firstGroup = row.group; else if (row.group != firstGroup) headed = true;
            }
            for each (var row:Object in source) {
                if (row.type == "launcher") continue;
                if (modID && !inPage(row)) continue;
                if (modID && headed && row.group != lastGroup) {
                    // Headers are list entries so scrolling stays uniform; selection skips them.
                    lastGroup = row.group;
                    // Inside a "Tab - Section" section the sidebar already names the tab.
                    var groupTitle:String = String(row.groupTitle), split:int = groupTitle.indexOf(" - ");
                    if (groupID && split > 0) groupTitle = groupTitle.substr(split + 3);
                    data.push({row:{type:"section", title:groupTitle, mod:row.mod, id:"@section/" + row.group, group:row.group, editable:false},
                        sText:"", uID:data.length, bDisabled:true, bShowSpinner:false, uCategory:0, bEnabled:false, bSubSetting:false,
                        uType:types.SDT_LINK, sliderData:{fValue:0, sDisplayValue:""}, stepperData:{aStepperOptions:[], uIndex:0}, checkBoxData:{bChecked:false}});
                }
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
            var rebuilt:Boolean = settingsList.setEntries(data, preserve);
            if (rebuilt) hoverIndex = -1;
            // Home's detail card and empty state show current keys, which come from vanilla Controls.
            var homeKeys:Boolean = homeEmptyState();
            for each (mod in mods) if (homePage() && mod.hotkeys.length) homeKeys = true;
            if (hasHotkeys || homeKeys) nativeHotkeys.open();
            var openKey:String = "";
            for each (row in allRows) if (row.mod == "osfsettings" && row.key == "openMenu" && row.type == "hotkey") openKey = String(row.value || "");
            homeEmpty.show(openKey);
            selected = data.length ? Math.max(0, Math.min(selected, data.length - 1)) : -1;
            if (selected >= 0 && data[selected].row.type == "section") selected = settingFrom(selected, 1, -1);
            if (rebuilt) {
                options.selectedIndex = selected;
                options.scrollPosition = Math.min(scroll, options.maxScrollPosition);
            }
            if (homePage() && launcher.hasEntries && !data.length && !nav.focused) launcher.focused = true;
            syncInput();
            if (!searching()) menuStage.focus = nav.focused ? nav : launcherPage() ? launcher : options as MovieClip;
            empty.visible = !expandedLauncher() && !homeEmptyState();
            MenuStyle.setText(empty, data.length ? "" : bindingsPage() ? keybindings.emptyText : issuesPage() ? tr("menu.noIssues") : tr("menu.noSettings"));
            var title:String = bindingsPage() ? tr("menu.keybindings") : issuesPage() ? tr("menu.issues") : tr("menu.home"); var summary:String = "";
            for each (var mod:Object in mods) if (mod.mod == modID) { title = mod.title; summary = mod.summary; }
            MenuStyle.fit(heading, title.toUpperCase());
            MenuStyle.fit(headerSummary, summary);
            MenuStyle.setText(section, launcher.visible ? tr("home.interfaces") : issuesPage() ? tr("menu.reportedIssues") : homePage() ? tr("home.mods") : "");
            MenuStyle.setText(count, tr("counts.issues", {count:data.length}));
            MenuStyle.setText(headerCount, String(launcher.visible ? launcher.count : data.length));
            headerCount.x = section.x + section.textWidth + 18;
            MenuStyle.setText(homeCount, String(data.length));
            homeCount.x = homeMods.x + homeMods.textWidth + 18;
            if (!modID && (!bindingsPage() || !preserve)) setStatus(issuesPage() ? tr("menu.issuesHint") : "");
            else if (!preserve) setStatus(tr("menu.autoSave"));
            decorate();
            refreshing = false; updateSelection();
        }
        // Previous and next walk the sidebar: the open mod's sections, then the next page or mod.
        private function changePage(direction:int):void
        {
            if (pageLocked()) return;
            var item:Object = nav.move(direction);
            if (!item) return;
            var inPage:Boolean = !nav.focused;
            openEntry(item);
            if (inPage) { nav.focused = true; focusNav(false); }
        }
        private function current():Object { return launcherPage() ? launcher.current : options && options.selectedEntry ? options.selectedEntry.row : null; }
        private function syncInput():void
        {
            var modal:Boolean = Boolean(captureRow) || editingString() || confirmingAction();
            var disabled:Boolean = closing || modal || bindingBusy() || nav.focused || launcherPage();
            options.disableInput = disabled || searching();
            options.disableSelection = disabled;
            MovieClip(options).mouseEnabled = MovieClip(options).mouseChildren = !modal;
            nav.mouseChildren = !editingString() && !confirmingAction();
            MovieClip(bar).visible = !confirmingAction();
        }
        private function updateSelection():void
        {
            syncInput();
            var row:Object = current();
            settingsDetails.visible = Boolean(modID);
            if (settingsDetails.visible) {
                settingsDetails.show(row, options.y, editingString());
                stringEditor.y = settingsDetails.editorTop;
            }
            homeDetails.visible = homePage() && !homeEmptyState();
            if (homeDetails.visible) homeDetails.show(homeDetail(row), launcher.visible ? launcher.y : MenuStyle.LIST_TOP);
            issueDetails.visible = issuesPage();
            issueDetails.show(issueDetails.visible ? row : null);
            if (bindingsPage()) keybindings.showSelection(row);
            updateFooter(row);
        }
        private function acceptLabel(row:Object):String
        {
            if (editingString()) return tr("buttons.save");
            if (captureRow) return tr("buttons.confirmBinding");
            if (bindingsPage()) return tr("buttons.changeBinding");
            if (homeEmptyState()) return tr("menu.keybindings");
            if (!modID) return tr("buttons.open");
            if (row) switch (row.type) {
                case "action": return tr("buttons.runAction");
                case "string": return tr("buttons.editText");
                case "key":
                case "hotkey": return tr("buttons.changeBinding");
                case "enum": return tr("buttons.nextChoice");
            }
            return tr("buttons.toggle");
        }
        private function updateFooter(row:Object):void
        {
            footerDirty = false;
            var busy:Boolean = bindingBusy(), editing:Boolean = editingString(), reporting:Boolean = issuesPage();
            var resetVisible:Boolean = Boolean(!captureRow && !busy && modID && row && row.editable && row.type != "hotkey" && row.type != "action");
            var clearVisible:Boolean = Boolean(!captureRow && !busy && (modID || bindingsPage()) && row &&
                (row.type == "key" && row.allowUnbound && Number(row.value) != 255 || row.type == "hotkey" && nativeHotkeys.canClear));
            var acceptText:String = acceptLabel(row);
            var backText:String = editing || captureRow || nativeHotkeys.busy ? tr("buttons.cancel") : expandedLauncher() && !launcher.locked ? tr("home.showLess") : tr("buttons.back");
            var acceptVisible:Boolean = !busy && (captureRow ? captureReady : Boolean(row && (!modID || row.editable && ACCEPT_TYPES[row.type])));
            if (reporting) {
                acceptVisible = Boolean(row && row.nexusModId);
                acceptText = tr("buttons.openModPage");
                resetVisible = clearVisible = issueDetails.scrollable;
            }
            if (bindingsPage()) {
                resetVisible = false;
                acceptVisible = Boolean(!busy && !searching() && row && row.editable);
                clearVisible = clearVisible && !searching();

            }
            if (launcherPage()) {
                acceptText = row && row.more ? tr("home.expand") : tr("buttons.open");
                acceptVisible = Boolean(row && row.editable);
                resetVisible = clearVisible = false;
            }
            if (homeEmptyState()) acceptVisible = !busy;
            if (editing) resetVisible = clearVisible = false;
            if (nav.focused && !captureRow && !busy) {
                acceptText = tr("buttons.open"); backText = tr("buttons.back");
                acceptVisible = homeEmptyState() || options.entryCount > 0 || homePage() && launcher.hasEntries;
                resetVisible = clearVisible = false;
            }
            // Compute final states first; each native button is updated at most once.
            var changed:Boolean = updateButton("Accept", acceptText, acceptVisible);
            changed = updateButton("Cancel", backText, !busy) || changed;
            changed = updateButton("YButton", reporting ? tr("buttons.scrollUp") : tr("buttons.reset"), resetVisible) || changed;
            changed = updateButton("XButton", reporting ? tr("buttons.scrollDown") : tr("buttons.clearBinding"), clearVisible) || changed;
            var pages:Boolean = !captureRow && !busy && !searching() && !editing;
            changed = updateButton("LShoulder", tr("buttons.previousPage"), pages) || changed;
            changed = updateButton("RShoulder", tr("buttons.nextPage"), pages) || changed;
            if (changed) bar.RefreshButtons();
            layoutFooter();
        }
        private function layoutFooter():void
        {
            // Native button widths can settle on a later frame.
            var width:Number = CONFIG::largeText ? MenuStyle.RIGHT - MenuStyle.LEFT :
                Math.max(320, MenuStyle.RIGHT - MovieClip(bar).width - MenuStyle.LEFT - 48);
            if (footerWidth == width && footerText == status.text) return;
            footerWidth = width; footerText = status.text;
            status.width = width;
            status.height = Math.ceil(status.textHeight) + 8;
            status.y = CONFIG::largeText ? MenuStyle.FOOTER_LINE + 8 : MenuStyle.FOOTER_Y - status.height / 2;
        }
        // The card describes whichever Home item is selected: a mod, an interface or SHOW ALL.
        private function homeDetail(row:Object):Object
        {
            if (!row) return null;
            if (row.more) return {title:row.title, badge:row.badge, tint:Badge.MORE, subtitle:"", description:row.hint,
                warning:"", chips:[], hotkeys:[]};
            if (row.type == "launcher") return {title:row.title, badge:Badge.initials(String(row.title)),
                tint:row.editable ? Badge.color(row.mod + "/" + row.key) : MenuStyle.LINE,
                subtitle:row.modTitle && row.modTitle != row.title ? row.modTitle : "", description:row.hint,
                warning:row.editable ? "" : String(row.message || tr("home.unavailable")),
                chips:row.editable ? [tr("home.interface")] : [tr("home.interface"), tr("home.unavailable")], hotkeys:[]};
            var hotkeys:Array = [];
            for each (var hotkey:Object in row.hotkeys) hotkeys.push({title:hotkey.title, key:hotkey.value || tr("values.unboundTitle")});
            return {title:row.title, badge:Badge.initials(String(row.title)), tint:Badge.color(String(row.mod)), subtitle:"",
                description:row.hint, warning:"", chips:row.chips, hotkeys:hotkeys};
        }
        private function selectionChanged(event:Event):void
        {
            if (refreshing || skipSection()) return;
            if (!nav.focused && homePage() && !expandedLauncher()) focusLauncher(false);
            else updateSelection();
        }
        private function focusSound(event:Event):void { Object(definition("Shared.GlobalFunc")).PlayMenuSound("UIMenuGeneralFocus"); }
        private function itemPressed(event:Event):void { accept(); }
        private function accept():void
        {
            if (confirmingAction() || launcherPage() && launcherAcceptHeld) return;
            if (editingString()) { saveString(); return; }
            if (bindingBusy() || searching() || frame <= searchExitFrame + 1) return;
            if (captureRow) { confirmBinding(); return; }
            if (closing || launching || refreshing || requestedRefresh || activationFrame == frame || settingsList.dragging) return;
            activationFrame = frame;
            if (nav.focused) { enterPage(); return; }
            if (homeEmptyState()) { openEntry({kind:"page", id:"bindings"}); return; }
            var row:Object = current(); if (!row) return;
            if (issuesPage()) {
                if (row.nexusModId) setStatus(BGSCodeObj.openIssueModPage(row.mod, row.id) ? tr("issues.modPageOpened") : tr("errors.openModPage"));
                return;
            }
            if (launcherPage()) { if (row.more) launcher.toggleExpanded(); else if (row.editable) launch(row); }
            else if (!modID && !bindingsPage()) { openEntry({kind:"mod", id:row.mod}); nav.focused = true; focusNav(false); }
            else if (row.type == "bool") options.OnEntryPressed();
            else if (row.type == "key" && row.editable) beginBinding(row);
            else if (row.type == "hotkey" && row.editable) nativeHotkeys.press();
            else if (row.type == "string" && row.editable) beginString(row);
            else if (row.type == "action" && row.editable) beginAction(row);
            else if (row.type == "enum" && row.editable) {
                var clip:Object = options.FindClipForEntry(options.selectedIndex);
                if (clip) clip.LargeStepper_mc.PressHandler();
            }
        }
        private function reset():void
        {
            if (searching() || editingString() || confirmingAction() || current() && current().type == "action") return;
            if (issuesPage()) { issueDetails.scroll(-160); return; }
            if (captureRow || bindingBusy() || current() && current().type == "hotkey") return;
            var row:Object = current();
            if (!settingsList.dragging && modID && row && row.editable && row.value != row.defaultValue) edit(row, row.defaultValue);
        }
        private function valueChanged(event:Event):void
        {
            event.stopPropagation();
            if (refreshing || modalBusy()) return;
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
            if (row.type == "action" || confirmingAction()) return;
            if (closing || refreshing || bindingBusy() || options.scrollbarScrolling || !row.editable) return;
            activationFrame = frame;
            if (value == row.value) return;
            var result:Object;
            if (row.type == "float") result = BGSCodeObj.setFloat(row.mod, row.key, Number(value));
            else if (row.type == "int") result = BGSCodeObj.setInt(row.mod, row.key, String(value));
            else if (row.type == "enum") result = BGSCodeObj.setEnum(row.mod, row.key, String(value));
            else if (row.type == "string") result = BGSCodeObj.setString(row.mod, row.key, String(value), Number(StringSetting.byteLength(String(value))));
            else if (row.type == "key") result = BGSCodeObj.setKey(row.mod, row.key, Number(value));
            else result = BGSCodeObj.setBool(row.mod, row.key, Boolean(value));
            if (result && result.ok) {
                if (row.type == "key") row.valueName = NumericSetting.text(row, value);
                row.value = value; updateSelection();
            }
            if (result && result.ok) setStatus(tr("menu.autoSave"));
            else setStatus(result ? result.error : tr("errors.save"), true);
            requestedRefresh = true;
        }
        private function beginAction(row:Object):void
        {
            if (!row.confirmation) { invokeAction(row); return; }
            menuStage.focus = null;
            actionConfirmation.open(row, actionAcceptHeld); syncInput();
        }
        private function finishActionConfirmation(row:Object, run:Boolean):void
        {
            searchExitFrame = activationFrame = frame;
            if (run) invokeAction(row);
            else { refresh(); setStatus(tr("actions.cancelled")); }
        }
        private function invokeAction(row:Object):void
        {
            var result:Object = BGSCodeObj.invokeAction(row.mod, row.key);
            // Refresh from the service, including handlers which completed immediately.
            refresh();
            if (result && result.ok) setStatus(tr("actions.submitted"));
            else setStatus(result && result.error ? result.error : tr("errors.submitAction"), true);
        }
        private function beginString(row:Object):void
        {
            if (!BGSCodeObj.textInput(true)) return;
            stringConfirmHeld = false;
            stringEditor.open(row); updateSelection();
            setStatus(tr("strings.hint"));
        }
        private function saveString():void
        {
            if (!editingString() || frame <= activationFrame + 1) return;
            if (!stringEditor.valid) {
                stringEditor.showError(tr("strings.limit", {limit:stringEditor.row.maxLength}));
                menuStage.focus = stringEditor.input; return;
            }
            var row:Object = stringEditor.row;
            var value:String = stringEditor.input.text;
            var result:Object = BGSCodeObj.setString(row.mod, row.key, value, Number(StringSetting.byteLength(value)));
            if (!result || !result.ok) {
                stringEditor.showError(result ? result.error : tr("errors.saveText"));
                menuStage.focus = stringEditor.input; return;
            }
            finishString(false);
        }
        private function finishString(cancel:Boolean):void
        {
            stringEditor.close(); stringConfirmHeld = false;
            BGSCodeObj.textInput(false);
            searchExitFrame = activationFrame = frame;
            refresh();
            setStatus(cancel ? tr("strings.unchanged") : tr("menu.autoSave"));
        }
        private function launch(row:Object):void
        {
            // 0 rejected, 1 closing (handoff queued), 2 loading (the card shows LOADING until pollLaunch resolves).
            var result:int = int(BGSCodeObj.launch(row.mod, row.key));
            if (result == 2) {
                launching = row;
                launcher.loading = row;
                launcher.mouseEnabled = launcher.mouseChildren = false;
                setStatus(tr("home.loadingStatus", {title:String(row.title)}));
            } else if (result == 1) lockForClose();
            else setStatus(tr("home.loadFailed", {title:String(row.title)}), true);
        }
        // Native is closing the menu (or handing off to a launched one); take no more input.
        private function lockForClose():void
        {
            closing = true; syncInput();
            launcher.mouseEnabled = launcher.mouseChildren = false;
        }
        private function pollLaunch():void
        {
            var result:Object = BGSCodeObj.pollLaunch();
            if (result.state == "pending") return;
            // Keep the loading caption until native's queued hide removes the menu.
            if (result.state == "closing") { lockForClose(); return; }
            var row:Object = launching;
            endLaunch();
            setStatus(String(result.message) || tr("home.loadFailed", {title:String(row.title)}), true);
        }
        private function endLaunch():void
        {
            launching = null;
            launcher.loading = null;
            launcher.mouseEnabled = launcher.mouseChildren = launcher.visible;
        }
        private function launcherAccept(pressed:Boolean):void
        {
            if (pressed) launcherAcceptHeld = true;
            else if (launcherAcceptHeld) {
                launcherAcceptHeld = false;
                accept(); // Hand off after the launch button is released.
            }
        }
        private function back():void
        {
            if (confirmingAction()) { actionConfirmation.cancel(); return; }
            if (editingString()) { finishString(true); return; }
            if (nativeHotkeys.busy) { nativeHotkeys.cancel(); return; }
            if (nativeHotkeys.saving) return;
            if (captureRow) { finishBinding(true); return; }
            if (frame <= searchExitFrame + 1) return;
            if (launching) { // Leave both Settings and Pause; the provider keeps opening.
                lockForClose(); BGSCodeObj.close(); return;
            }
            if (closing || settingsList.dragging || requestedRefresh) return;
            if (expandedLauncher() && !launcher.locked) { launcher.toggleExpanded(); return; }
            // Back leaves the page for the sidebar; from the sidebar it closes the menu.
            if (!nav.focused) { focusNav(true); return; }
            lockForClose(); BGSCodeObj.close();
        }
        public function ProcessUserEvent(name:String, pressed:Boolean):Boolean
        {
            if (name == "Accept") actionAcceptHeld = pressed;
            if (!initialized || closing) return false;
            if (pressed) noteMove(name);
            if (confirmingAction()) return actionConfirmation.userEvent(name, pressed);
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
            if (routeNavigation(name, pressed)) return true;
            if (nav.focused) return false;
            if (bar.ProcessUserEvent(name, pressed)) return true;
            if (launcherPage()) return false;
            var clip:Object = options.FindClipForEntry(options.selectedIndex);
            return clip && clip.IsSlider() ? Boolean(clip.Slider_mc.ProcessUserEvent(name, pressed)) : false;
        }
        private function hoverEntry(event:MouseEvent):void
        {
            var index:int = -1;
            if (event.type == MouseEvent.MOUSE_OVER)
                for (var target:DisplayObject = event.target as DisplayObject; target && target != options; target = target.parent)
                    if ("itemIndex" in target) { index = Object(target).itemIndex; break; }
            if (index != hoverIndex) { hoverIndex = index; decorate(); }
        }
        // Vanilla ignores the wheel while the list's input is off, which it is whenever the
        // sidebar has focus. Scroll the page anyway; focus and selection stay where they are.
        private function wheelList(event:MouseEvent):void
        {
            if (!nav.focused || !event.delta || !initialized || closing || refreshing) return;
            if (modalBusy() || settingsList.dragging) return;
            var next:int = Math.max(0, Math.min(options.scrollPosition + (event.delta < 0 ? 1 : -1), options.maxScrollPosition));
            if (next != options.scrollPosition) { options.scrollPosition = next; decorate(); }
            event.stopPropagation();
        }
        // A click that misses the vanilla arrow catchers reaches the row press, which always
        // advances. Resolve the arrow columns by position so either side steps its own way.
        private function clickStepper(event:MouseEvent):void
        {
            if (!modID || !initialized || closing || refreshing || requestedRefresh || settingsList.dragging || modalBusy()) return;
            var entry:DisplayObject = event.target as DisplayObject;
            while (entry && entry != options && !("itemIndex" in entry)) entry = entry.parent;
            if (!entry || entry == options) return;
            var item:Object = options.GetDataForEntry(Object(entry).itemIndex);
            var stepper:Object = Object(entry).LargeStepper_mc;
            if (!item || item.row.type != "enum" || !item.row.editable || !stepper.visible) return;
            var left:Rectangle = DisplayObject(stepper.LeftCatcher_mc).getBounds(menuStage);
            var right:Rectangle = DisplayObject(stepper.RightCatcher_mc).getBounds(menuStage);
            var code:uint = event.stageX >= left.left && event.stageX <= left.right ? Keyboard.LEFT :
                event.stageX >= right.left && event.stageX <= right.right ? Keyboard.RIGHT : 0;
            if (!code) return;
            event.stopImmediatePropagation();
            // The keyboard path steps, wraps, plays the vanilla sound, and reports the change.
            Object(entry).onKeyDownHandler(new KeyboardEvent(KeyboardEvent.KEY_DOWN, true, true, 0, code));
        }
        private function mouseFocus(event:MouseEvent):void
        {
            if (event.type == MouseEvent.MOUSE_OVER && (!launcherPage() || expandedLauncher())) return;
            CONFIG::testHarness { if (event.type == MouseEvent.MOUSE_DOWN) testMouseDown = testMouseEvent(event, testMouseDown); }
            if (modalBusy() || !initialized || closing) return;
            var target:DisplayObject = event.target as DisplayObject;
            if (event.type == MouseEvent.MOUSE_DOWN && target) {
                // Pressing anywhere on the page moves focus there; the footer buttons act on either.
                if (nav.contains(target)) { focusNav(true); return; }
                if (nav.focused && !MovieClip(bar).contains(target)) focusNav(false);
                // GFx can focus the input before this bubbling mouse-down arrives.
                // Entering the page must not replace that focus with the results list.
                if (bindingsPage() && target == keybindings.search) { menuStage.focus = keybindings.search; return; }
            }
            if (homePage() && target && launcher.contains(target)) { focusLauncher(true); return; }
            if (!target || !MovieClip(options).contains(target)) return;
            if (homePage()) focusLauncher(false);
            menuStage.focus = options as MovieClip; syncInput();
            while (target && target != options) {
                if ("itemIndex" in target) { options.selectedIndex = Object(target).itemIndex; break; }
                target = target.parent;
            }
        }
        private function keyDown(event:KeyboardEvent):void
        {
            if (event.keyCode == Keyboard.ENTER) actionAcceptHeld = true;
            if (confirmingAction()) { confirmationKey(event, true); return; }
            if (editingString()) {
                if (event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.ESCAPE) {
                    if (event.keyCode == Keyboard.ENTER && frame > activationFrame + 1) stringConfirmHeld = true;
                    event.stopImmediatePropagation(); event.preventDefault();
                }
                return;
            }
            if (bindingBusy() || captureRow) { event.stopImmediatePropagation(); event.preventDefault(); return; }
            if (keybindings && keybindings.searchKey(event)) return;
            var input:String = keyboardName(event.keyCode);
            noteMove(input);
            if (!initialized || closing || refreshing || requestedRefresh || settingsList.dragging) return;
            if (input && input != "Cancel" && routeNavigation(input, true)) {
                event.stopImmediatePropagation(); event.preventDefault(); return;
            }
            if (nav.focused) return;
            if (event.keyCode == Keyboard.B) reset();
            else if (event.keyCode == Keyboard.X && (issuesPage() || current() && (current().type == "key" || current().type == "hotkey"))) clearBinding();
            else if (issuesPage() && (input == "PageUp" || input == "PageDown")) issueDetails.scroll(input == "PageUp" ? -360 : 360);
            else if (input == "Left" || input == "Right") {
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
            if (event.keyCode == Keyboard.ENTER) actionAcceptHeld = false;
            if (confirmingAction()) { confirmationKey(event, false); return; }
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
            if (event.keyCode == Keyboard.ENTER && (nav.focused || launcherPage())) {
                routeNavigation("Accept", false); event.stopImmediatePropagation(); event.preventDefault(); return;
            }
            if (frame <= searchExitFrame + 1 || captureRow || bindingBusy() || event.keyCode == Keyboard.ENTER) event.stopImmediatePropagation();
        }
        private function confirmationKey(event:KeyboardEvent, pressed:Boolean):void
        {
            var name:String = keyboardName(event.keyCode);
            event.stopImmediatePropagation(); event.preventDefault();
            if (name) actionConfirmation.userEvent(name, pressed);
        }
        private function routeNavigation(name:String, pressed:Boolean):Boolean
        {
            // Page navigation responds on press in either focus area. Consume the
            // release too so the footer's button handler cannot navigate again.
            if (name == "LShoulder" || name == "RShoulder") {
                if (pressed) changePage(name == "LShoulder" ? -1 : 1);
                return true;
            }
            if (nav.focused) return navigate(name, pressed);
            if (launcherPage() && name == "Accept") { launcherAccept(pressed); return true; }
            if (launcherPage() && navigateLauncher(name, pressed)) return true;
            if (homePage() && launcher.hasEntries && name == "Up" && options.selectedIndex <= 0) {
                if (pressed && navigationFrame != frame) { navigationFrame = frame; focusLauncher(true); }
                return true;
            }
            if (bindingsPage() && navigateBindings(name, pressed)) return true;
            return false;
        }
        private function keyboardName(code:uint):String
        {
            switch (code) {
                case Keyboard.UP: return "Up";
                case Keyboard.DOWN: return "Down";
                case Keyboard.LEFT: return "Left";
                case Keyboard.RIGHT: return "Right";
                case Keyboard.PAGE_UP: return "PageUp";
                case Keyboard.PAGE_DOWN: return "PageDown";
                case Keyboard.ENTER: return "Accept";
                case Keyboard.ESCAPE: return "Cancel";
                case LEFT_BRACKET: return "LShoulder";
                case RIGHT_BRACKET: return "RShoulder";
            }
            return "";
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
        private function navigateLauncher(name:String, pressed:Boolean):Boolean
        {
            if (name != "Up" && name != "Down" && name != "Left" && name != "Right" && name != "PageUp" && name != "PageDown") return false;
            if (pressed && !refreshing && !requestedRefresh && navigationFrame != frame) {
                navigationFrame = frame;
                if (name == "Down" && !launcher.expanded && options.entryCount &&
                    launcher.onLastRow) {
                    focusLauncher(false); options.selectedIndex = 0;
                }
                else launcher.navigate(name);
            }
            return true;
        }
        private function advance(event:Event):void
        {
            ++frame;
            if (initialized && !closing) {
                if (launching) pollLaunch();
                nativeHotkeys.advance();
                keybindings.advance(allRows,bindingBusy());
                syncInput();
                if (footerDirty) updateFooter(current());
                if (captureRow) pollBinding();
                else if (!editingString() && !confirmingAction()) {
                    if (getTimer() >= nextRevisionPoll && !bindingBusy() && !settingsList.dragging && !searching()) {
                        nextRevisionPoll = getTimer() + 250;
                        if (String(BGSCodeObj.revision()) != revision) requestedRefresh = true;
                    }
                    if (requestedRefresh && !bindingBusy() && !settingsList.dragging) refresh();
                    else decorate();
                }
                // Captures and editors can change status without refreshing the list;
                // native button widths can also settle after RefreshButtons returns.
                layoutFooter();
            }
            CONFIG::testHarness { advanceTestObservations(); }
        }
        private function clearBinding():void
        {
            if (searching() || editingString() || confirmingAction()) return;
            if (issuesPage()) { issueDetails.scroll(160); return; }
            if (current() && current().type == "hotkey") { nativeHotkeys.clearBinding(); return; }
            var row:Object = current();
            if (!captureRow && modID && row && row.type == "key" && row.allowUnbound) edit(row, 255);
        }
        private function beginBinding(row:Object):void
        {
            var result:Object = BGSCodeObj.beginKeyCapture(row.mod, row.key);
            if (!result || !result.ok) { setStatus(tr("errors.capture"), true); return; }
            captureRow = row; captureRow.capturing = true; captureReady = false;
            CONFIG::testHarness { testCaptureState = "waiting"; }
            syncInput();
            menuStage.focus = null;
            decorate();
            var clip:MovieClip = options.FindClipForEntry(options.selectedIndex) as MovieClip;
            clip.addChild(captureBinding);
            captureBinding.x = MenuStyle.LIST_WIDTH - 296; captureBinding.y = (MenuStyle.ROW_HEIGHT - captureBinding.height) / 2;
            Object(captureBinding).SetBinding({aButtonName:[], aPCKeyName:[]});
            Object(captureBinding).SetState("listening"); captureBinding.visible = true;
            setStatus(tr("bindings.capture"));
            updateSelection();
        }
        private function pollBinding():void
        {
            var state:Object = BGSCodeObj.pollKeyCapture();
            CONFIG::testHarness { testCaptureState = state ? state.state : "unavailable"; }
            if (!state || state.state == "cancelled" || state.state == "idle") { finishBinding(true); return; }
            if (state.state == "candidate" || state.state == "confirmed") {
                Object(captureBinding).SetBinding({aButtonName:[], aPCKeyName:[state.name]});
                if (!captureReady && state.released) {
                    captureReady = true; updateSelection(); confirmBinding();
                } else if (state.state == "confirmed") confirmBinding();
            }
        }
        private function confirmBinding():void
        {
            if (!captureRow || !captureReady) return;
            var result:Object = BGSCodeObj.commitKeyCapture();
            if (result && result.ok) { finishBinding(false); return; }
            setStatus(result && result.error ? result.error : tr("errors.save"), true);
        }
        private function focusLost(event:Event):void
        {
            actionAcceptHeld = launcherAcceptHeld = false;
            if (confirmingAction()) actionConfirmation.cancel();
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
            activationFrame = frame;
            setStatus(cancel ? tr("bindings.unchanged") : tr("menu.autoSave"));
            refresh();
        }
        private function decorate():void
        {
            settingsList.render(nativeHotkeys, rowHeight(), Boolean(modID), bindingsPage(), !launcherPage() && !nav.focused, hoverIndex);
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
