package
{
    import flash.display.BitmapData;
    import flash.display.DisplayObject;
    import flash.display.DisplayObjectContainer;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Matrix;
    import flash.geom.Rectangle;
    import flash.text.Font;
    import flash.text.TextField;
    import flash.ui.Keyboard;
    import flash.utils.ByteArray;
    import flash.utils.getTimer;

    // Opt-in checks against the real movie and vanilla list, using -Pverify=true
    // with the design fixture. No operating-system input or game files are used.
    public final class PreviewChecks
    {
        private var menu:MovieClip;
        private var list:Object;
        private var playbackTab:DisplayObject;
        private var tick:int = 0;
        private var step:int = -1;
        private var setter:Function;
        private var bindingsOnly:Boolean;
        private var captures:Boolean;
        private var waitingSince:int;
        private var readyFrames:int = 0;
        private var launcher:Object;
        private var originalRows:Function;
        private var launchState:String = "pending";
        private var originalIssueOpener:Function;
        private var issueOpenCount:int = 0;
        private var issueOpenResult:Boolean = true;
        private var openedIssue:String = "";
        private var originalControlMap:Object;
        private var issueButtonRestColor:uint;
        private var panelBridge:Object;
        private var panelRows:Array;
        private var panelGeneration:int = 0;
        private var panelWrites:int = 0;
        private var panelActions:int = 0;
        private var panelBaseline:int = 0;
        private var modPageRows:Function;
        private var launchedInterface:String = "";

        public function PreviewChecks(movie:MovieClip, bindings:Boolean = false, screenshots:Boolean = false)
        {
            bindingsOnly = bindings;
            captures = screenshots;
            waitingSince = getTimer();
            menu = movie;
            for (var i:int = 0; i < menu.numChildren; ++i) {
                var child:Object = menu.getChildAt(i);
                if ("selectedIndex" in child && "InitializeEntries" in child) list = child;
            }
            menu.addEventListener(Event.ENTER_FRAME, advance);
        }
        private function require(value:Boolean, message:String):void
        {
            if (!value) throw new Error(message);
            trace("[verify] OK " + message);
        }
        private function userEvent(name:String):void
        {
            Object(menu).ProcessUserEvent(name, true); Object(menu).ProcessUserEvent(name, false);
        }
        private function homeDetail():Object { return Object(findNamed(menu,"homeDetails")).detail; }
        private function navFocused():Boolean { return Object(findNamed(menu,"navigation")).focused; }
        // A mod's groups share one list under headers, so rows are found by key.
        private function indexOf(key:String):int
        {
            for (var i:int = 0; i < list.entryCount; ++i) if (list.GetDataForEntry(i).row.key == key) return i;
            return -1;
        }
        private function searchField():TextField { return DisplayObjectContainer(findNamed(menu,"bindingSearch")).getChildAt(0) as TextField; }
        private function key(code:uint):void
        {
            list.dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_DOWN, true, true, 0, code));
        }
        private function advance(event:Event):void
        {
            // Capture mode keeps the original visual settling interval. Routine
            // assertions advance on rendered frames and observable async results.
            if (captures && ++tick % 12) return;
            try {
                if (getTimer() - waitingSince > 2000) throw new Error("step " + step + " did not become ready");
                if (!readyForStep()) { readyFrames = 0; return; }
                // Allow the menu's activation-frame guard and list rendering to
                // finish before the next synthetic input event.
                if (!captures && ++readyFrames < 2) return;
                readyFrames = 0;
                waitingSince = getTimer();
                switch (step++) {
                case -1:
                    require(Object(menu).startupPhase == "ready" && list != null, "menu ready at root");
                    require(list.entryCount == 3 && list.selectedEntry.row.mod == "design-preview", "Home opens with the settings list selected");
                    require(findNamed(menu, "mods") != null && findNamed(menu, "issues") != null, "both root tabs are reachable");
                    checkLocalization();
                    PreviewGameplayChecks.run(menu, require);
                    require(navFocused(), "menu opens in the sidebar");
                    capture("all-mods");
                    if (bindingsOnly) step = 34;
                    else { userEvent("Accept"); require(!navFocused(), "Accept enters Home from the sidebar"); userEvent("Accept"); }
                    break;
                case 0:
                    require(Object(menu).startupPhase == "ready" && list != null, "menu ready");
                    requireGlyphs("SELECTED SETTING");
                    requireGlyphs("Auto-advance stages");
                    require(list.entryCount == 19 && list.GetDataForEntry(0).row.type == "section", "the mod's four groups share one list under headers");
                    require(list.selectedEntry.row.key == "autoAdvance", "first setting selected");
                    require(findInput(menu) == null, "menu has no text input");
                    key(Keyboard.F);
                    require(menu.stage.focus == list && !list.disableInput, "F leaves list input active");
                    playbackTab = findNamed(menu, "Playback");
                    require(playbackTab != null, "playback tab present");
                    capture("default"); userEvent("Accept"); break;
                case 1:
                    require(list.selectedEntry.row.value == false, "toggle saved and reread");
                    require(findNamed(menu, "Playback") == playbackTab, "toggle reuses tabs");
                    capture("changed"); key(Keyboard.B); break;
                case 2:
                    require(list.selectedEntry.row.value == true, "reset restored schema default");
                    require(findNamed(menu, "Playback") == playbackTab, "reset reuses tabs");
                    findNamed(menu, "Camera").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 3:
                    require(list.selectedEntry.row.key == "freeCamera" && list.entryCount == 2 && navFocused(), "a sidebar section lists only its own settings");
                    checkShoulderNavigation(true);
                    userEvent("Right"); step = 114; break;
                case 114:
                    checkShoulderNavigation(false);
                    userEvent("Cancel");
                    require(navFocused(), "Back returns from a browsed section to the sidebar");
                    key(221); step = 4; break;
                case 4:
                    require(list.selectedEntry.row.key == "hotkeys" && list.entryCount == 3, "next page walks to the next section");
                    key(221); break;
                case 5:
                    require(list.selectedEntry.row.key == "debug", "the last section is reachable");
                    key(221); break;
                case 6:
                    require(list.selectedEntry.row.mod != "design-preview" && list.selectedEntry.row.type == "hotkey", "next page continues to the next mod");
                    key(219); break;
                case 7:
                    require(list.selectedEntry.row.key == "autoAdvance", "previous page reopens the mod at its top");
                    userEvent("Accept"); break;
                case 8:
                    require(!navFocused() && !list.disableInput, "Accept enters the page");
                    userEvent("Cancel"); break;
                case 9:
                    require(!navFocused() && list.selectedEntry.row.key == "autoAdvance", "Done ends panel editing without leaving the page");
                    userEvent("Cancel"); step = 115; break;
                case 115:
                    require(navFocused() && list.selectedEntry.row.key == "autoAdvance", "Back returns to the sidebar and keeps the page");
                    userEvent("Accept"); list.selectedIndex = indexOf("restoreView"); step = 10; break;
                case 10:
                    require(list.selectedEntry.row.key == "restoreView", "last row reachable");
                    capture("last-row");
                    setter = Object(menu).BGSCodeObj.setBool;
                    Object(menu).BGSCodeObj.setBool = function(mod:String, key:String, value:Boolean):Object {
                        return {ok:false, error:"Could not save this setting. Your previous value is unchanged."};
                    };
                    userEvent("Accept"); break;
                case 11:
                    require(list.selectedEntry.row.value == true, "save failure preserves previous value");
                    Object(menu).BGSCodeObj.setBool = setter;
                    findNamed(menu, "Controls").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 12:
                    userEvent("Right"); list.selectedIndex = indexOf("toggleKey"); step = 116; break;
                case 116:
                    require(!list.disableInput && Object(menu).BGSCodeObj.pollKeyCapture().state != "waiting",
                        "entering a panel with a binding selected does not start capture");
                    userEvent("Accept"); step = 13; break;
                case 13:
                    require(list.disableInput && Object(menu).BGSCodeObj.pollKeyCapture().state == "waiting", "key capture disables list input");
                    capture("key-waiting");
                    Object(menu).BGSCodeObj.previewKey(116, true); break;
                case 14:
                    userEvent("Accept");
                    require(list.selectedEntry.row.value == 115, "held candidate cannot be confirmed");
                    findNamed(menu, "Camera").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    require(list.selectedEntry.row.key == "toggleKey", "capture blocks tab changes");
                    userEvent("RShoulder"); userEvent("LShoulder");
                    require(list.selectedEntry.row.key == "toggleKey" && list.disableInput, "capture blocks shoulder page changes");
                    Object(menu).BGSCodeObj.previewKey(116, false); break;
                case 15:
                    capture("key-saved"); break;
                case 16:
                    require(list.selectedEntry.row.value == 116 && !list.disableInput, "confirmed key saved and list input restored");
                    userEvent("YButton"); break;
                case 17:
                    require(list.selectedEntry.row.value == 115, "key reset restores default");
                    userEvent("XButton"); break;
                case 18:
                    require(list.selectedEntry.row.value == 255, "optional key can be cleared");
                    capture("key-unbound"); userEvent("YButton"); break;
                case 19:
                    list.selectedIndex = indexOf("advanceKey"); userEvent("XButton");
                    require(list.selectedEntry.row.value == 13, "required key cannot be cleared");
                    userEvent("Accept"); break;
                case 20:
                    Object(menu).BGSCodeObj.previewKey(9, true); break;
                case 21:
                    require(Object(menu).BGSCodeObj.pollKeyCapture().name == "Tab" && list.disableInput, "Tab is captured instead of navigating back");
                    Object(menu).BGSCodeObj.previewKey(27, true);
                    Object(menu).BGSCodeObj.previewKey(27, false); break;
                case 22:
                    require(list.selectedEntry.row.value == 13 && !list.disableInput, "Escape preserves previous binding");
                    list.selectedIndex = indexOf("toggleKey"); userEvent("Accept"); break;
                case 23:
                    Object(menu).BGSCodeObj.previewKey(13, true);
                    Object(menu).BGSCodeObj.previewKey(13, true); break;
                case 24:
                    require(list.selectedEntry.row.value == 115 && list.disableInput, "held Enter does not save itself");
                    setter = Object(menu).BGSCodeObj.setKey;
                    Object(menu).BGSCodeObj.setKey = function(mod:String, key:String, value:Number):Object {
                        return {ok:false, error:"Could not save this setting. Your previous value is unchanged."};
                    };
                    Object(menu).BGSCodeObj.previewKey(13, false); break;
                case 25: break;
                case 26:
                    require(list.selectedEntry.row.value == 115 && list.disableInput, "failed key save preserves binding and candidate");
                    capture("key-save-failed");
                    Object(menu).BGSCodeObj.setKey = setter;
                    userEvent("Accept"); break;
                case 27:
                    require(list.selectedEntry.row.value == 13 && !list.disableInput, "failed key save can be retried");
                    userEvent("Accept"); break;
                case 28:
                    Object(menu).BGSCodeObj.previewKey(32, true);
                    Object(menu).BGSCodeObj.previewKey(32, false); break;
                case 29: break;
                case 30:
                    require(list.selectedEntry.row.value == 32 && list.selectedEntry.row.valueName == "Space",
                        "Space keeps its virtual-key identity and display label");
                    userEvent("Accept"); break;
                case 31:
                    Object(menu).BGSCodeObj.previewKey(179, true);
                    Object(menu).BGSCodeObj.previewKey(179, false); break;
                case 32: break;
                case 33:
                    require(list.selectedEntry.row.value == 179 && list.selectedEntry.row.valueName == "Key 0xB3",
                        "unlisted key stays bindable with a fallback display label");
                    capture("key-fallback");
                    userEvent("Cancel"); break;
                case 34:
                    findNamed(menu,"bindings").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 35:
                    require(list.entryCount == 41,"MainGameplay rows include both sources, unbound actions and hidden reservations");
                    require(list.selectedEntry.row.action == "Jump","binding results start at the first native action");
                    require(!findNamed(menu,"clearBindingFilters").visible && !findNamed(menu,"bindingKeyFilter").visible,"filter controls stay hidden without a filter");
                    capture("keybindings");
                    var keyboardStyle:Object = menu.loaderInfo.applicationDomain.getDefinition("MenuStyle");
                    var keyboardType:Object = menu.loaderInfo.applicationDomain.getDefinition("KeyboardMap");
                    capture("keybindings-keyboard",new Rectangle(keyboardStyle.LEFT,keyboardStyle.SECTION_TOP,23 * keyboardType.KEY_WIDTH,keyboardType.HEIGHT),1);
                    capture("keybindings-legend",new Rectangle(keyboardStyle.LEFT + 15.5 * keyboardType.KEY_WIDTH,
                        keyboardStyle.SECTION_TOP,7.5 * keyboardType.KEY_WIDTH,keyboardType.HEIGHT),2);
                    var firstSearch:TextField = searchField();
                    menu.stage.focus = firstSearch;
                    firstSearch.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_DOWN,true));
                    require(menu.stage.focus == firstSearch,"first search click retains input focus when leaving the sidebar");
                    firstSearch.dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_UP,true,true,0,Keyboard.ENTER));
                    findNamed(menu,"key_77").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 36:
                    require(list.entryCount == 3,"key filter includes plain and chord bindings");
                    require(findNamed(menu,"bindingKeyFilter").visible && findNamed(menu,"clearBindingFilters").visible,"key filter shows its chip and clear control");
                    findNamed(menu,"key_77").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 37:
                    require(list.entryCount == 41 && !findNamed(menu,"clearBindingFilters").visible,"clicking selected key clears the filter");
                    findNamed(menu,"key_162").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 38:
                    require(list.entryCount == 1 && list.selectedEntry.row.action == "preview/chord","modifier filter finds chord");
                    capture("keybindings-chord");
                    findNamed(menu,"bindingKeyFilter").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    require(list.entryCount == 41 && !findNamed(menu,"bindingKeyFilter").visible,"key chip clears the key filter");
                    var search:TextField = searchField();
                    menu.stage.focus = search;
                    search.text = "alternate"; search.dispatchEvent(new Event(Event.CHANGE)); break;
                case 39:
                    require(list.entryCount == 1,"two slots of one action appear once");
                    require(menu.stage.focus == searchField(),"filter refresh preserves search focus");
                    key(221); key(Keyboard.B); key(Keyboard.X); userEvent("Accept"); userEvent("RShoulder");
                    require(list.entryCount == 1 && list.selectedEntry.row.action == "preview/alternate","search suppresses native and raw shortcuts");
                    var input:TextField = searchField();
                    var originalTextInput:Function = Object(menu).BGSCodeObj.textInput;
                    var textInputEnabled:Boolean = true;
                    Object(menu).BGSCodeObj.textInput = function(enabled:Boolean):void {
                        textInputEnabled = enabled; originalTextInput(enabled);
                    };
                    try {
                        // This proves Flash cleanup when the event is delivered;
                        // Windows focus-event delivery needs separate game evidence.
                        menu.stage.dispatchEvent(new Event(Event.DEACTIVATE));
                        require(menu.stage.focus == list && !textInputEnabled,
                            "Flash deactivation leaves search and releases native text input");
                        require(searchField().text == "alternate" && list.entryCount == 1 && findNamed(menu,"bindings") != null,
                            "Flash deactivation preserves the search query and page");
                    } finally { Object(menu).BGSCodeObj.textInput = originalTextInput; }
                    menu.stage.focus = input;
                    input.dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_DOWN,true,true,0,Keyboard.ENTER));
                    input.dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_UP,true,true,0,Keyboard.ENTER));
                    require(menu.stage.focus == list,"Enter returns focus to results"); break;
                case 40:
                    userEvent("Right");
                    capture("keybindings-alternate");
                    var query:TextField = searchField();
                    menu.stage.focus = query; query.text = "zzzz-no-match"; query.dispatchEvent(new Event(Event.CHANGE)); break;
                case 41:
                    require(list.entryCount == 0,"combined search has explicit empty results");
                    capture("keybindings-empty");
                    var focused:TextField = searchField();
                    focused.dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_DOWN,true,true,0,Keyboard.ENTER));
                    focused.dispatchEvent(new KeyboardEvent(KeyboardEvent.KEY_UP,true,true,0,Keyboard.ENTER));
                    require(menu.stage.focus == list && findNamed(menu,"bindings") != null,"Enter leaves empty search without closing menu");
                    findNamed(menu,"clearBindingFilters").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 42:
                    findNamed(menu,"bindingSource").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 43:
                    require(list.entryCount == 33,"Game source filter includes reserved bindings");
                    var mouse:TextField = searchField();
                    mouse.text = "mouse"; mouse.dispatchEvent(new Event(Event.CHANGE)); break;
                case 44:
                    require(list.entryCount == 1 && list.selectedEntry.row.action == "Attack","mouse button zero is bound and searchable");
                    capture("keybindings-mouse");
                    findNamed(menu,"clearBindingFilters").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 45:
                    list.selectedIndex = list.entryCount - 1;
                    list.scrollPosition = list.maxScrollPosition; break;
                case 46:
                    require(list.selectedEntry.row.action == "CameraPath" && list.selectedEntry.row.reserved &&
                        !list.selectedEntry.row.editable && list.selectedEntry.row.value == "F12","hidden F12 owner is reachable and read only");
                    capture("keybindings-scroll");
                    checkBindingPolicy();
                    setter = Object(menu).BGSCodeObj.pollBindings;
                    Object(menu).BGSCodeObj.pollBindings = function():Object {
                        var old:Object = setter(); return {generation:old.generation - 1,state:"ready",records:old.records};
                    };
                    findNamed(menu,"bindings").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 47:
                    require(list.entryCount == 0,"stale snapshot cannot populate a new request");
                    Object(menu).BGSCodeObj.pollBindings = setter; break;
                case 48:
                    require(list.entryCount == 41,"current snapshot replaces loading state");
                    searchField().text = "intentionally long"; searchField().dispatchEvent(new Event(Event.CHANGE)); break;
                case 49:
                    require(list.entryCount == 1 && list.selectedEntry.row.action == "preview/long","long labels remain selectable with both native slots");
                    capture("keybindings-long-label");
                    findNamed(menu,"bindingSource").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    findNamed(menu,"bindingSource").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 50:
                    require(list.entryCount == 1 && list.selectedEntry.row.mod == "navigation","individual mod filter combines with search");
                    findNamed(menu,"clearBindingFilters").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    searchField().text = "unbound action"; searchField().dispatchEvent(new Event(Event.CHANGE)); break;
                case 51:
                    var unbound:Object = null;
                    for (var index:int = 0; index < list.entryCount; ++index) {
                        if (list.GetDataForEntry(index).row.action == "preview/unbound") {
                            unbound = list.GetDataForEntry(index).row; list.selectedIndex = index; break;
                        }
                    }
                    require(unbound && unbound.editable && unbound.records[0].key == 255,"unbound native action remains editable");
                    capture("keybindings-unbound");
                    searchField().text = "media"; searchField().dispatchEvent(new Event(Event.CHANGE)); break;
                case 52:
                    require(list.entryCount == 1 && list.selectedEntry.row.records[0].key == 179,"unsupported diagram key remains searchable and editable");
                    capture("keybindings-unsupported");
                    findNamed(menu,"clearBindingFilters").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    list.selectedIndex = indexOf("Jump");
                    userEvent("Right");
                    Object(menu).BGSCodeObj.previewGamepad(true); step = 100; break;
                case 100:
                    require(list.entryCount == 40 && list.selectedEntry.row.action == "Jump", "controller publication preserves the selected action");
                    require(list.selectedEntry.row.records.length == 1 && list.selectedEntry.row.records[0].device == 2,
                        "controller rows exclude keyboard and mouse records from mixed snapshots");
                    require(!findNamed(menu,"key_77").parent.visible, "controller display hides the keyboard");
                    var nativeHost:DisplayObjectContainer = list.FindClipForEntry(list.selectedIndex) as DisplayObjectContainer;
                    var primary:DisplayObject = findNamed(nativeHost,"MainBinding_mc");
                    require(!findNamed(nativeHost,"AltBinding_mc").visible && Object(primary.parent).activePriority == 0,
                        "controller display hides Alternate and selects Primary");
                    require(Object(primary).Icon_mc.Icon_tf.text == "A" && Object(primary).Icon_mc.Icon_tf.visible,
                        "native controller row renders the A glyph");
                    capture("keybindings-controller");
                    list.selectedIndex = indexOf("preview/chord"); break;
                case 101:
                    capture("keybindings-controller-chord");
                    searchField().text = "shoulder"; searchField().dispatchEvent(new Event(Event.CHANGE));
                    Object(menu).BGSCodeObj.previewGamepad(false); step = 103; break;
                case 103:
                    require(searchField().text == "shoulder" && !findNamed(menu,"bindingKeyFilter").visible,
                        "device switch retains search and clears controller button filter");
                    findNamed(menu,"clearBindingFilters").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 104:
                    require(list.entryCount == 41 && findNamed(menu,"key_77").parent.visible,
                        "keyboard display and all rows return after switching devices");
                    require(findNamed(list.FindClipForEntry(list.selectedIndex),"AltBinding_mc").visible,
                        "keyboard publication restores Alternate");
                    Object(menu).BGSCodeObj.previewGamepad(true); break;
                case 105:
                    list.selectedIndex = indexOf("Jump");
                    Object(menu).BGSCodeObj.previewAllowNativeCapture(true);
                    userEvent("Accept");
                    Object(menu).BGSCodeObj.previewGamepad(false, true); break;
                case 106:
                    require(Object(findNamed(menu,"keybindingsPage")).gamepad && list.disableInput,
                        "device publication is deferred while native capture owns input");
                    Object(menu).BGSCodeObj.previewRebind("Jump", 121, "F10"); break;
                case 107:
                    require(Object(findNamed(menu,"keybindingsPage")).gamepad && list.disableInput,
                        "device publication remains deferred until native save completes");
                    require(Object(menu).BGSCodeObj.previewControlsSaved() == 1,
                        "repeated validation and batched save completion issue only one save"); break;
                case 108:
                    require(!list.disableInput && !Object(findNamed(menu,"keybindingsPage")).gamepad,
                        "save completion applies the pending device publication");
                    require(list.selectedEntry.row.value == "F10" && list.selectedEntry.row.records.length == 1 &&
                        list.selectedEntry.row.records[0].key == 121,
                        "saved binding replaces its label and excludes the old unbound-slot key");
                    primary = findNamed(list.FindClipForEntry(list.selectedIndex),"MainBinding_mc");
                    require(Object(primary).PCKey_mc.PCKey_tf.text.indexOf("F10") >= 0,
                        "native binding cell redraws the saved key without reopening the page");
                    Object(menu).BGSCodeObj.previewAllowNativeCapture(false);
                    findNamed(menu,"issues").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); step = 53; break;
                case 53:
                    var issueCount:int = Object(menu).BGSCodeObj.getIssues().length;
                    require(issueCount > 0 && list.entryCount == issueCount, "all fixture reports appear on the issues page");
                    requireGlyphs("Issues: " + issueCount);
                    userEvent("Right");
                    capture("issues");
                    originalIssueOpener = Object(menu).BGSCodeObj.openIssueModPage;
                    Object(menu).BGSCodeObj.openIssueModPage = function(mod:String, id:String):Boolean {
                        ++issueOpenCount; openedIssue = mod + "/" + id; return issueOpenResult;
                    };
                    for (var issueIndex:int = 0; issueIndex < list.entryCount; ++issueIndex)
                        if (list.GetDataForEntry(issueIndex).row.id == "dependency:osfui") list.selectedIndex = issueIndex;
                    require(list.selectedEntry.row.nexusModId == 17711, "dependency warning retains its mod page destination");
                    step = 109; break;
                case 109:
                    require(issueOpenCount == 0, "selecting a dependency warning does not open the browser");
                    require(findNamed(menu,"issueModPage") != null &&
                        findText(menu, menu.loaderInfo.applicationDomain.getDefinition("Localization").text("issues.requiredVersion")) != null,
                        "a version gap with a mod page shows the versions and a prominent mod page button");
                    var issueButton:DisplayObject = findNamed(menu,"issueModPage");
                    var issueHint:Object = findNamed(menu,"issueConfirmHint");
                    require(issueHint.visible && issueHint.PCButton_mc.ButtonInstance_mc.PCButton_tf.text == "E",
                        "detail button shows the native keyboard Accept binding");
                    issueButtonRestColor = buttonFill(issueButton);
                    issueButton.dispatchEvent(new MouseEvent(MouseEvent.ROLL_OVER));
                    require(buttonFill(issueButton) != issueButtonRestColor, "detail button has visible hover feedback");
                    capture("issue-mod-page-hover");
                    issueButton.dispatchEvent(new MouseEvent(MouseEvent.ROLL_OUT));
                    require(buttonFill(issueButton) == issueButtonRestColor, "hover feedback clears when the pointer leaves");
                    userEvent("Cancel");
                    require(!issueHint.visible, "detail Accept hint hides while the sidebar owns confirmation");
                    userEvent("Right");
                    require(issueHint.visible && issueOpenCount == 0, "entering the issue shows its hint without opening a page");
                    issueControlMap(1,"Xenon_A"); step = 120; break;
                case 120:
                    issueHint = findNamed(menu,"issueConfirmHint");
                    var expectedGlyph:String = Object(menu.loaderInfo.applicationDomain.getDefinition("Shared.GlobalFunc")).NameToTextMap["Xenon_A"];
                    require(issueHint.ConsoleButton_mc.ButtonInstance_mc.visible &&
                        issueHint.ConsoleButton_mc.ButtonInstance_mc.ConsoleButton_tf.text == expectedGlyph &&
                        !issueHint.PCButton_mc.ButtonInstance_mc.visible,
                        "detail button follows the native controller Accept glyph on device switch");
                    capture("issue-mod-page-controller");
                    Object(menu).ProcessUserEvent("Accept",true); break;
                case 121:
                    require(issueOpenCount == 0 && buttonFill(findNamed(menu,"issueModPage")) != issueButtonRestColor,
                        "holding controller Accept shows pressed feedback without an early browser launch");
                    capture("issue-mod-page-pressed");
                    Object(menu).ProcessUserEvent("Accept",false);
                    issueControlMap(0,"F8"); break;
                case 122:
                    issueHint = findNamed(menu,"issueConfirmHint");
                    require(issueHint.PCButton_mc.ButtonInstance_mc.visible &&
                        issueHint.PCButton_mc.ButtonInstance_mc.PCButton_tf.text == "F8",
                        "detail button follows a rebound keyboard Accept key");
                    issueControlMap(-1,""); step = 110; break;
                case 110:
                    require(issueOpenCount == 1 && openedIssue == "osfsettings/dependency:osfui",
                        "Open mod page sends the selected issue identity exactly once");
                    require(findText(menu, menu.loaderInfo.applicationDomain.getDefinition("Localization").text("issues.modPageOpened")) != null,
                        "successful mod page opening displays feedback");
                    capture("issue-mod-page-opened");
                    issueOpenResult = false; userEvent("Accept"); break;
                case 111:
                    require(issueOpenCount == 2 && findText(menu, menu.loaderInfo.applicationDomain.getDefinition("Localization").text("errors.openModPage")) != null,
                        "failed mod page opening displays an error and permits retry");
                    capture("issue-mod-page-failed");
                    issueOpenResult = true;
                    var modPage:DisplayObject = findNamed(menu,"issueModPage");
                    modPage.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_DOWN, true));
                    require(issueOpenCount == 3 && openedIssue == "osfsettings/dependency:osfui",
                        "the detail mod page button opens on press even when GFx drops the click");
                    modPage.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_UP, true));
                    modPage.dispatchEvent(new MouseEvent(MouseEvent.CLICK, true));
                    require(issueOpenCount == 3, "a full detail button click does not open a second browser page");
                    modPage.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_DOWN, true));
                    require(issueOpenCount == 4, "a new detail button press can open the page again");
                    modPage.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_UP, true));
                    modPage.dispatchEvent(new MouseEvent(MouseEvent.CLICK, true));
                    require(issueOpenCount == 4, "the repeated detail button click opens only once");
                    for (issueIndex = 0; issueIndex < list.entryCount; ++issueIndex)
                        if (list.GetDataForEntry(issueIndex).row.id == "dependency:sample-pack") list.selectedIndex = issueIndex;
                    break;
                case 112:
                    require(!list.selectedEntry.row.nexusModId && findNamed(menu,"issueModPage") == null &&
                        findText(menu, menu.loaderInfo.applicationDomain.getDefinition("Localization").text("issues.notInstalled")) != null,
                        "a missing dependency without a mod page shows the gap but no button");
                    capture("issue-dependency-missing");
                    userEvent("Accept"); break;
                case 113:
                    require(issueOpenCount == 4, "accepting an issue without a mod page never opens the browser");
                    for (issueIndex = 0; issueIndex < list.entryCount; ++issueIndex)
                        if (list.GetDataForEntry(issueIndex).row.id == "long-description") list.selectedIndex = issueIndex;
                    Object(menu).BGSCodeObj.openIssueModPage = originalIssueOpener;
                    step = 118; break;
                case 118:
                    // One more frame so the long report repaints before it is scrolled.
                    step = 78; break;
                case 78:
                    // Let the selected row repaint before capturing the scrolled report.
                    var issuePane:Object = findNamed(menu,"issueDetails");
                    require(issuePane.scrollable, "long reports scroll within the wider details pane");
                    issuePane.scroll(10000);
                    require(list.selectedEntry.row.id == "long-description", "scrolling report details preserves issue selection");
                    capture("issues-long");
                    step = 54;
                    findNamed(menu,"mods").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 54:
                    for (var childIndex:int = 0; childIndex < menu.numChildren; ++childIndex) {
                        var candidate:Object = menu.getChildAt(childIndex);
                        if ("shelfHeight" in candidate && "toggleExpanded" in candidate) launcher = candidate;
                    }
                    require(launcher && launcher.visible && !launcher.expanded && list.visible, "Home combines interfaces and mod settings");
                    require(findNamed(menu,"launcher") == null, "separate Launcher tab removed");
                    // Two columns of three cards; the sixth opens all ten interfaces.
                    var moreCard:String = "launcher_5";
                    require(launcher.columns == 2 && findNamed(menu,moreCard) != null && findNamed(menu,"launcher_6") == null &&
                        Object(findNamed(menu,moreCard)).row.more && Object(findNamed(menu,moreCard)).row.title == "SHOW ALL 10",
                        "Home fills three mission-card rows and adds Show All");
                    require(Object(findNamed(menu,"launcher_0")).row.title == "Absolute Control", "recent order overrides alphabetical order");
                    require(TextField(DisplayObjectContainer(findNamed(menu,"mods")).getChildAt(0)).text == "HOME", "Home tab label");
                    requireGlyphs("INTERFACES"); requireGlyphs("MODS");
                    list.selectedIndex = 0;
                    require(list.selectedEntry.row.mod == "design-preview", "select the first mod before navigating to the shelf");
                    require(list.selectedEntry.row.summary == "13 SETTINGS  |  2 ACTIONS" && list.GetDataForEntry(1).row.summary == "4 HOTKEYS",
                        "mod rows summarize settings, actions and hotkeys");
                    require(homeDetail().title == "OSF Animation" && homeDetail().chips.join() == "13 SETTINGS,2 ACTIONS", "detail card describes the selected mod");
                    userEvent("Right");
                    require(!navFocused(), "Right enters Home from the sidebar");
                    capture("home"); userEvent("Up"); break;
                case 55:
                    require(launcher.focused && list.disableInput, "Up from the first mod focuses the interface shelf");
                    require(launcher.current.title == "Absolute Control" && homeDetail().title == "Absolute Control", "detail card follows the focused interface");
                    capture("home-interface");
                    launcher.navigate("Down");
                    require(!launcher.onLastRow, "middle mission-card row keeps Down inside the shelf");
                    launcher.navigate("Down");
                    require(launcher.onLastRow, "third mission-card row can return to mod settings");
                    userEvent("Down"); break;
                case 56:
                    require(!launcher.focused && !list.disableInput, "Down from the shelf returns to mod settings");
                    findNamed(menu,"launcher_5").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 57:
                    require(launcher.expanded && !list.visible, "Show more expands interfaces inside Home");
                    require(launcher.current.type == "launcher" && launcher.focused, "expanded grid selects an actual interface");
                    require(findNamed(menu,"launcher_" + launcher.columns).y > findNamed(menu,"launcher_0").y &&
                        findNamed(menu,"launcher_" + (launcher.columns - 1)).y == findNamed(menu,"launcher_0").y,
                        "expanded grid keeps the shelf's columns");
                    capture("home-expanded"); userEvent("PageDown"); break;
                case 58:
                    require(launcher.scrollPosition == (launcher.capacity < 10 ? launcher.capacity : 0), "Page Down reaches the remaining interfaces");
                    require(launcher.current.title == "Unavailable Interface" && homeDetail().warning == "This interface is unavailable in the current context.",
                        "unavailable interfaces explain why");
                    capture("home-unavailable");
                    userEvent("Cancel"); break;
                case 59:
                    require(!launcher.expanded && list.visible, "Back collapses interfaces without leaving Home");
                    originalRows = Object(menu).BGSCodeObj.getRows;
                    Object(menu).BGSCodeObj.getRows = function():Array {
                        var rows:Array = originalRows();
                        for each (var item:Object in rows) if (item.type == "launcher" && item.mod == "launcher-preview-8") item.recentOrder = 100;
                        return rows;
                    };
                    Object(menu).BGSCodeObj.revision = function():String { return "promoted"; }; break;
                case 60: break;
                case 61:
                    require(Object(findNamed(menu,"launcher_0")).row.title == "Ship Planner", "new native history moves the last-opened interface first");
                    Object(menu).BGSCodeObj.getRows = function():Array {
                        return originalRows().filter(function(row:Object, index:int, source:Array):Boolean { return row.type != "launcher"; });
                    };
                    Object(menu).BGSCodeObj.revision = function():String { return "no-interfaces"; }; break;
                case 62: break;
                case 63:
                    require(!launcher.visible && list.visible && list.y == menu.loaderInfo.applicationDomain.getDefinition("MenuStyle").LIST_TOP && !list.disableInput, "no interfaces restores the full settings list");
                    capture("home-no-interfaces");
                    Object(menu).BGSCodeObj.getRows = function():Array {
                        // OSF Settings' own hotkey alone leaves Home with nothing to list.
                        return [{type:"hotkey", mod:"osfsettings", modTitle:"OSF Settings", modDescription:"", group:"general", groupTitle:"General",
                            key:"openMenu", action:"osfsettings/openMenu", registered:true, title:"Open mod settings", hint:"", defaultName:"Unbound"}];
                    };
                    Object(menu).BGSCodeObj.revision = function():String { return "empty"; }; break;
                case 64: break;
                case 65:
                    require(findNamed(menu,"homeEmpty").visible && !list.visible && !launcher.visible, "OSF Settings alone shows the empty state");
                    capture("home-empty");
                    findNamed(menu,"homeKeybindings").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 66:
                    require(findNamed(menu,"bindingSearch").parent.visible, "empty state links to Keybindings");
                    Object(menu).BGSCodeObj.getRows = function():Array {
                        return originalRows().filter(function(row:Object, index:int, source:Array):Boolean { return row.type == "launcher"; });
                    };
                    Object(menu).BGSCodeObj.revision = function():String { return "interfaces-only"; };
                    findNamed(menu,"mods").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 67: break;
                case 68:
                    userEvent("Right");
                    require(launcher.visible && launcher.expanded && launcher.locked && launcher.focused && !list.visible && !findNamed(menu,"homeEmpty").visible,
                        "interfaces without mod settings fill Home");
                    require(TextField(DisplayObjectContainer(findNamed(menu,"mods")).getChildAt(0)).text == "HOME", "interfaces-only Home keeps its tab");
                    capture("home-interfaces-only");
                    Object(menu).BGSCodeObj.getRows = originalRows;
                    Object(menu).BGSCodeObj.revision = function():String { return "restored"; }; break;
                case 69: break;
                case 70:
                    require(launcher.visible && !launcher.expanded && list.visible && list.entryCount == 3, "mod settings bring back the shelf and list");
                    list.selectedIndex = 1;
                    require(homeDetail().hotkeys.length == 4 && homeDetail().chips.length == 0, "hotkey-only mods list their keys instead of a count");
                    capture("home-hotkeys");
                    list.selectedIndex = 0; userEvent("Accept"); break;
                case 71:
                    findNamed(menu,"Advanced").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 72:
                    var advanced:int = indexOf("debug");
                    require(advanced == 0 && list.entryCount == 4 &&
                        list.GetDataForEntry(advanced + 1).row.key == "rescan" && list.GetDataForEntry(advanced + 2).row.key == "resetIndex" &&
                        list.GetDataForEntry(advanced + 3).row.key == "language", "settings and actions preserve declaration order");
                    userEvent("Right"); list.selectedIndex = advanced + 1; step = 117; break;
                case 117:
                    require(list.selectedEntry.row.actionState == "Run", "entering a panel leaves its selected action untouched");
                    userEvent("YButton");
                    require(list.selectedEntry.row.actionState == "Run", "reset leaves action controls unchanged");
                    userEvent("Accept"); step = 73; break;
                case 73:
                    require(list.selectedEntry.row.key == "rescan" && list.selectedEntry.row.actionState == "Completed", "inline action completes and preserves selection");
                    list.selectedIndex = indexOf("resetIndex"); userEvent("Accept"); break;
                case 74:
                    require(list.disableInput, "inline confirmation blocks the underlying list");
                    userEvent("Accept"); break;
                case 75:
                    require(!list.disableInput && list.selectedEntry.row.actionState == "Run", "confirmation defaults to Cancel without invoking");
                    userEvent("Accept"); break;
                case 76:
                    userEvent("Right"); userEvent("Accept"); break;
                case 77:
                    require(!list.disableInput && list.selectedEntry.row.key == "resetIndex" && list.selectedEntry.row.actionState == "Completed", "confirmed inline action completes");
                    list.selectedIndex = indexOf("language");
                    require(list.selectedEntry.row.type == "string", "setting after actions remains reachable");
                    capture("mixed-controls");
                    startPanelFixture(); step = 200; break;
                case 200:
                    findNamed(menu,"Advanced").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    list.selectedIndex = indexOf("debug"); break;
                case 201:
                    require(navFocused() && list.selectedEntry.row.key == "debug" && list.selectedEntry.row.value == false,
                        "panel regression fixture begins in sidebar browse state");
                    userEvent("Accept"); userEvent("Accept");
                    require(!navFocused() && panelWrites == 0 && list.selectedEntry.row.value == false,
                        "Confirm enters editing without changing the first value or leaking a repeated activation"); break;
                case 202:
                    list.selectedIndex = indexOf("panelDisabled"); key(Keyboard.RIGHT);
                    require(panelWrites == 0 && list.selectedEntry.row.value == false, "disabled rows cannot save during a panel edit session");
                    list.selectedIndex = indexOf("language");
                    require(findInput(menu) == null, "passing a string row does not open its editor");
                    list.selectedIndex = indexOf("panelKey");
                    require(!list.disableInput && Object(menu).BGSCodeObj.pollKeyCapture().state != "waiting", "passing a key row does not capture input");
                    list.selectedIndex = indexOf("rescan");
                    require(list.selectedEntry.row.actionState == "Completed" && panelActions == 0, "passing an action does not invoke it again");
                    list.selectedIndex = indexOf("panelNumber"); key(Keyboard.RIGHT); break;
                case 203:
                    require(list.selectedEntry.row.key == "panelNumber" && list.selectedEntry.row.value == "3" && panelWrites == 1,
                        "panel editing survives disabled, text, key and action rows and adjusts the next numeric row");
                    list.selectedIndex = indexOf("panelEnum"); step = 218; break;
                case 218:
                    key(Keyboard.RIGHT); step = 204; break;
                case 204:
                    require(list.selectedEntry.row.key == "panelEnum" && list.selectedEntry.row.value == "second" && panelWrites == 2,
                        "moving from a numeric row to an enum keeps editing active (key=" + list.selectedEntry.row.key +
                        ", value=" + list.selectedEntry.row.value + ", writes=" + panelWrites + ")");
                    clickPanelLabel();
                    require(panelWrites == 2 && list.selectedEntry.row.value == "second", "an active enum row label cannot change its value");
                    var previousEnum:MovieClip = list.FindClipForEntry(list.selectedIndex) as MovieClip;
                    Object(previousEnum).LargeStepper_mc.LeftCatcher_mc.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_OVER, true));
                    key(Keyboard.DOWN);
                    require(list.selectedEntry.row.key == "panelKey", "Down leaves the enum while its arrow is hovered");
                    // GFx retargets the stationary pointer to the old row when
                    // leaving edit mode disables that clip's mouse children.
                    previousEnum.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_OVER, true));
                    require(list.selectedEntry.row.key == "panelKey" && panelWrites == 2,
                        "a stationary pointer retarget cannot undo keyboard navigation or save a value");
                    list.selectedIndex = indexOf("debug"); key(Keyboard.RIGHT); break;
                case 205:
                    require(list.selectedEntry.row.key == "debug" && list.selectedEntry.row.value == true && panelWrites == 3,
                        "moving back to a boolean keeps editing active after saves refresh the rows");
                    userEvent("Cancel");
                    require(!navFocused(), "Done stays in the settings panel");
                    list.selectedIndex = indexOf("panelNumber"); key(Keyboard.RIGHT);
                    require(panelWrites == 3 && list.selectedEntry.row.value == "3", "Done stops edits while browsing other rows"); break;
                case 206:
                    userEvent("Cancel");
                    require(navFocused() && list.selectedEntry.row.key == "panelNumber", "a second Back returns to the sidebar and retains selection");
                    userEvent("Right");
                    require(!navFocused() && panelWrites == 3 && list.selectedEntry.row.value == "3", "Right enters navigation without stepping the selected numeric value"); break;
                case 207:
                    key(Keyboard.RIGHT);
                    require(!navFocused() && panelWrites == 3 && list.selectedEntry.row.value == "3", "repeated Right while browsing cannot change a setting");
                    userEvent("Left");
                    require(navFocused() && panelWrites == 3 && list.selectedEntry.row.value == "3", "Left after Right-entry returns to the sidebar without saving");
                    key(Keyboard.RIGHT); step = 219; break;
                case 219:
                    key(Keyboard.LEFT);
                    require(navFocused() && panelWrites == 3 && list.selectedEntry.row.value == "3", "raw arrow navigation also returns to the sidebar without saving");
                    userEvent("Right"); step = 220; break;
                case 220:
                    userEvent("Accept"); userEvent("Accept");
                    require(!navFocused() && panelWrites == 3 && list.selectedEntry.row.value == "3", "Confirm explicitly starts editing after Right-entry without changing the value");
                    step = 221; break;
                case 221:
                    key(Keyboard.RIGHT); step = 208; break;
                case 208:
                    require(list.selectedEntry.row.key == "panelNumber" && list.selectedEntry.row.value == "4" && panelWrites == 4,
                        "Confirm after Right-entry enables the next deliberate edit");
                    movePanelNumberFirst(); ++panelGeneration; step = 209; break;
                case 209:
                    require(list.selectedIndex == 0 && list.selectedEntry.row.key == "panelNumber" && list.selectedEntry.row.value == "4",
                        "reordered refresh retains selection by setting identity");
                    key(Keyboard.RIGHT); break;
                case 210:
                    require(list.selectedEntry.row.value == "5" && panelWrites == 5, "reordered refresh preserves the panel edit session");
                    panelRows = panelRows.filter(function(row:Object, index:int, source:Array):Boolean { return row.key != "panelNumber"; });
                    ++panelGeneration; break;
                case 211:
                    require(indexOf("panelNumber") < 0, "selected setting removal is reflected by the next refresh");
                    panelBaseline = panelWrites;
                    list.selectedIndex = indexOf("debug"); key(Keyboard.LEFT);
                    require(navFocused() && panelWrites == panelBaseline, "selected setting removal ends editing so Left returns to the sidebar without changing a value");
                    restorePanelFixture();
                    findNamed(menu,"mods").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 212:
                    require(list.entryCount == 3 && list.selectedEntry.row.mod == "design-preview" &&
                        list.selectedEntry.row.summary == "13 SETTINGS  |  2 ACTIONS", "panel fixture restores the original Home rows");
                    userEvent("Right");
                    MovieClip(list.FindClipForEntry(1)).dispatchEvent(new MouseEvent(MouseEvent.MOUSE_OVER, true));
                    require(list.selectedIndex == 1, "pooled rows restore vanilla hover selection on Home");
                    list.selectedIndex = 0;
                    modPageRows = Object(menu).BGSCodeObj.getRows;
                    Object(menu).BGSCodeObj.getRows = function():Array {
                        return modPageRows().concat({type:"launcher", mod:"design-preview", modTitle:"Design Preview", key:"studio",
                            title:"Open studio", hint:"Open this mod's interface.", recentOrder:0, editable:true, message:""});
                    };
                    Object(menu).BGSCodeObj.revision = function():String { return "mod-interface"; };
                    step = 222; break;
                case 222: break;
                case 223:
                    list.selectedIndex = 0; userEvent("Accept"); break;
                case 224:
                    var localization:Object = menu.loaderInfo.applicationDomain.getDefinition("Localization");
                    require(list.entryCount == 21 && list.GetDataForEntry(0).row.title == localization.text("home.interfaces") &&
                        list.GetDataForEntry(1).row.key == "@launcher/studio" && list.GetDataForEntry(1).row.actionState == localization.text("buttons.open") &&
                        list.GetDataForEntry(2).row.type == "section", "a mod's full page leads with its own interfaces");
                    Object(menu).BGSCodeObj.launch = function(mod:String, id:String):int { launchedInterface = mod + "/" + id; return 0; };
                    list.selectedIndex = 1;
                    Object(menu).ProcessUserEvent("Accept", true);
                    require(launchedInterface == "", "a mod page interface waits for Accept release");
                    Object(menu).ProcessUserEvent("Accept", false); break;
                case 225:
                    require(launchedInterface == "design-preview/studio", "a mod page Open button launches its interface");
                    Object(menu).BGSCodeObj.getRows = modPageRows;
                    Object(menu).BGSCodeObj.revision = function():String { return "mod-interface-removed"; };
                    findNamed(menu,"mods").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 226:
                    userEvent("Right"); list.selectedIndex = 0; step = 79; break;
                case 79:
                    Object(menu).BGSCodeObj.launch = function(mod:String, id:String):int { return 2; };
                    Object(menu).BGSCodeObj.pollLaunch = function():Object { return {state:launchState, message:"Preview launch failed"}; };
                    findNamed(menu,"launcher_0").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 80:
                    require(loadingCaptionVisible() && !launcher.mouseChildren, "pending launch shows Loading and blocks card input");
                    launchState = "failed"; break;
                case 81:
                    require(!loadingCaptionVisible() && launcher.mouseChildren, "failed launch restores the caption and card input");
                    findNamed(menu,"launcher_0").dispatchEvent(new MouseEvent(MouseEvent.ROLL_OVER));
                    findNamed(menu,"launcher_0").dispatchEvent(new MouseEvent(MouseEvent.MOUSE_OVER,true));
                    require(findText(menu,"Preview launch failed") != null, "launcher rejection survives hover after the loading card redraws");
                    findNamed(menu,"launcher_1").dispatchEvent(new MouseEvent(MouseEvent.ROLL_OVER));
                    require(findText(menu,"Preview launch failed") == null, "selecting a different launcher clears the previous rejection");
                    launchState = "pending";
                    findNamed(menu,"launcher_0").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 82:
                    require(loadingCaptionVisible(), "failed launch can be retried");
                    launchState = "closing"; break;
                case 83:
                    require(loadingCaptionVisible() && !launcher.mouseChildren, "successful handoff keeps Loading while hide is queued");
                    break;
                case 84:
                    require(loadingCaptionVisible() && !launcher.mouseChildren, "Loading remains visible across closing frames");
                    menu.removeEventListener(Event.ENTER_FRAME, advance);
                    trace("[verify] PASS"); break;
                }
            } catch (error:Error) {
                menu.removeEventListener(Event.ENTER_FRAME, advance); trace("[verify] FAIL " + error);
            }
        }
        private function readyForStep():Boolean
        {
            if (step == -1) return Object(menu).startupPhase == "ready" && list != null;
            if (step == 35 || step == 48) return list.entryCount == 41;
            if (step == 100 || step == 105) return Object(findNamed(menu,"keybindingsPage")).gamepad;
            if (step == 103 || step == 108) return !Object(findNamed(menu,"keybindingsPage")).gamepad;
            if (step == 61) {
                var first:Object = findNamed(menu,"launcher_0");
                return first && first.row.title == "Ship Planner";
            }
            if (step == 63) return !launcher.visible;
            if (step == 65) return findNamed(menu,"homeEmpty").visible;
            // Locked alone is left over from the empty state; wait for the interfaces to arrive.
            if (step == 68) return launcher.locked && launcher.visible;
            if (step == 70) return launcher.visible && !launcher.locked;
            if (step == 200) return indexOf("panelNumber") >= 0;
            if (step == 209) return list.entryCount && list.GetDataForEntry(0).row.previewGeneration == 2;
            if (step == 211) return list.entryCount && list.GetDataForEntry(0).row.previewGeneration == 3;
            if (step == 223) return launcher.count == 11;
            if (step == 224) return list.entryCount == 21;
            if (step == 226) return launcher.visible && launcher.count == 10;
            if (step == 212) return list.entryCount == 3 && list.selectedEntry.row.mod == "design-preview" &&
                list.selectedEntry.row.summary == "13 SETTINGS  |  2 ACTIONS";
            return true;
        }
        private function loadingCaptionVisible():Boolean
        {
            var localization:Object = menu.loaderInfo.applicationDomain.getDefinition("Localization");
            return findText(DisplayObjectContainer(findNamed(menu,"launcher_0")), localization.text("home.loading")) != null;
        }
        private function checkShoulderNavigation(sidebar:Boolean):void
        {
            // Entry settles in a previous verification step. The shoulder taps
            // themselves stay back-to-back to catch duplicate release handling.
            for (var i:int = 0; i < 4; ++i) {
                var name:String = i % 2 == 0 ? "RShoulder" : "LShoulder";
                var expected:String = i % 2 == 0 ? "hotkeys" : "freeCamera";
                var context:String = sidebar ? "sidebar" : "page";
                Object(menu).ProcessUserEvent(name, true);
                require(list.selectedEntry.row.key == expected, context + " shoulder tap " + i + " changes page on press");
                Object(menu).ProcessUserEvent(name, false);
                require(list.selectedEntry.row.key == expected && navFocused() == sidebar,
                    context + " shoulder release keeps the page and focus");
            }
        }
        private function startPanelFixture():void
        {
            var bridge:Object = Object(menu).BGSCodeObj;
            panelBridge = {getRows:bridge.getRows, revision:bridge.revision,
                setBool:bridge.setBool, setInt:bridge.setInt, setEnum:bridge.setEnum, invokeAction:bridge.invokeAction};
            panelRows = panelBridge.getRows();
            var advanced:Array = panelRows.filter(function(row:Object, index:int, source:Array):Boolean {
                return row.mod == "design-preview" && row.group == "Advanced";
            });
            panelRows = panelRows.filter(function(row:Object, index:int, source:Array):Boolean {
                return row.mod != "design-preview" || row.group != "Advanced";
            });
            var debug:Object = advanced.shift();
            debug.value = false;
            var disabled:Object = panelRow("panelDisabled", "bool", false); disabled.editable = false;
            var number:Object = panelRow("panelNumber", "int", "2"); number.minimum = "0"; number.maximum = "9";
            var choice:Object = panelRow("panelEnum", "enum", "first");
            choice.options = [{value:"first", label:"First"}, {value:"second", label:"Second"}];
            var binding:Object = panelRow("panelKey", "key", 115); binding.allowUnbound = true; binding.valueName = "F4";
            panelRows = panelRows.concat([debug, disabled, number, choice, binding], advanced);
            panelGeneration = 1; panelWrites = 0; panelActions = 0;
            bridge.getRows = function():Array {
                var copies:Array = [];
                for each (var row:Object in panelRows) {
                    var copy:Object = {};
                    for (var field:String in row) copy[field] = row[field];
                    copy.previewGeneration = panelGeneration;
                    copies.push(copy);
                }
                return copies;
            };
            bridge.revision = function():String { return "panel-" + panelGeneration; };
            bridge.setBool = function(mod:String, key:String, value:Boolean):Object { return savePanelValue(mod, key, value); };
            bridge.setInt = function(mod:String, key:String, value:String):Object { return savePanelValue(mod, key, value); };
            bridge.setEnum = function(mod:String, key:String, value:String):Object { return savePanelValue(mod, key, value); };
            bridge.invokeAction = function(mod:String, key:String):Object { ++panelActions; return panelBridge.invokeAction(mod, key); };
        }
        private function panelRow(key:String, type:String, value:*):Object
        {
            return {mod:"design-preview", modTitle:"OSF Animation", modDescription:"Panel regression fixture", group:"Advanced",
                groupTitle:"Advanced", key:key, type:type, title:key, hint:"Panel edit-session regression", value:value, editable:true,
                defaultKnown:false};
        }
        private function savePanelValue(mod:String, key:String, value:*):Object
        {
            for each (var row:Object in panelRows) if (row.mod == mod && row.key == key) {
                require(row.editable, "disabled panel settings never reach the save bridge");
                ++panelWrites; row.value = value; return {ok:true};
            }
            throw new Error("unexpected panel setting save: " + mod + "/" + key);
        }
        private function movePanelNumberFirst():void
        {
            var number:Object;
            for each (var row:Object in panelRows) if (row.key == "panelNumber") number = row;
            panelRows = panelRows.filter(function(item:Object, index:int, source:Array):Boolean { return item.key != "panelNumber"; });
            panelRows.unshift(number);
        }
        private function clickPanelLabel():void
        {
            var clip:DisplayObject = list.FindClipForEntry(list.selectedIndex) as DisplayObject;
            require(clip != null, "selected enum row has a rendered click target");
            clip.dispatchEvent(new MouseEvent(MouseEvent.MOUSE_DOWN, true, false, 24, 12));
            clip.dispatchEvent(new MouseEvent(MouseEvent.CLICK, true, false, 24, 12));
        }
        private function restorePanelFixture():void
        {
            var bridge:Object = Object(menu).BGSCodeObj;
            for (var field:String in panelBridge) bridge[field] = panelBridge[field];
            panelRows = null; panelBridge = null;
        }
        private function checkLocalization():void
        {
            var localization:Object = menu.loaderInfo.applicationDomain.getDefinition("Localization");
            require(localization.text("counts.issues", {count:4}) == "Issues: 4", "issue count substitutes a number");
            require(localization.text("counts.issues", {count:0}) == "Issues: 0", "zero count is preserved");
            require(localization.text("counts.issues") == "Issues: {count}", "omitted parameters leave tokens intact");
            try {
                localization.initialize({probe:"{name}: {count}/{count}; {missing}; {empty}.", "counts.issues":"報告: {count}"});
                require(localization.text("counts.issues", {count:4}) == "報告: 4", "translated count substitutes a number");
                require(localization.text("probe", {name:"$& $1 {count} 日本語", count:0, empty:""}) ==
                    "$& $1 {count} 日本語: 0/0; {missing}; .", "repeated tokens, literal values, empty values and unknown tokens are preserved");
                require(localization.text("counts.mods", {count:3}) == "Mods: 3", "missing translation uses English with parameters");
            } finally { localization.initialize(null); }
        }
        private function findText(container:DisplayObjectContainer, text:String):TextField
        {
            if (!container.visible) return null;
            for (var i:int = 0; i < container.numChildren; ++i) {
                var child:DisplayObject = container.getChildAt(i);
                if (!child.visible) continue;
                if (child is TextField && TextField(child).text == text) return child as TextField;
                if (child is DisplayObjectContainer) {
                    var result:TextField = findText(child as DisplayObjectContainer, text);
                    if (result) return result;
                }
            }
            return null;
        }
        private function requireGlyphs(text:String):void
        {
            var field:TextField = findText(menu, text);
            require(field != null, "custom text present: " + text);
            var embedded:Boolean = false;
            for each (var font:Font in Font.enumerateFonts(false)) {
                if (font.fontName == field.defaultTextFormat.font) embedded = true;
            }
            require(embedded, "custom text uses an embedded game font: " + text);
            // Right-aligned labels can render outside a fixed 96px sample.
            var first:BitmapData = new BitmapData(Math.ceil(field.width), Math.ceil(field.height), true, 0);
            var second:BitmapData = new BitmapData(first.width, first.height, true, 0);
            field.text = "M"; first.draw(field);
            field.text = "I"; second.draw(field);
            field.text = text;
            var visible:Boolean = !first.getColorBoundsRect(0xFF000000, 0, false).isEmpty() &&
                                  !second.getColorBoundsRect(0xFF000000, 0, false).isEmpty();
            var difference:Object = first.compare(second);
            var distinct:Boolean = difference is BitmapData;
            if (distinct) BitmapData(difference).dispose();
            first.dispose(); second.dispose();
            require(visible && distinct, "custom font renders distinct glyphs: " + text);
        }
        private function checkBindingPolicy():void
        {
            var policy:Object = menu.loaderInfo.applicationDomain.getDefinition("KeybindingsData");
            var native:Object = {sInputName:"same",uContextID:0,bGamepadEntry:false,bIsDivider:false,bReadOnly:false,bRequired:true,
                MainBinding:{aPCKeyName:["Localized key"],aButtonName:["glyph"]},AltBinding:{aPCKeyName:[],aButtonName:[]}};
            var joined:Array = policy.join([native], [{type:"hotkey",action:"same",registered:false,title:"Wrong owner"}],
                [{action:"same",context:1,device:0,slot:0,key:77,modifier:255},
                 {action:"same",context:0,device:1,slot:1,key:0,modifier:162}],function(row:Object):String { return "Native translated label"; });
            require(joined.length == 1 && joined[0].title == "Native translated label" && joined[0].source == "Game","only explicitly registered metadata changes native ownership and labels");
            require(joined[0].records.length == 1 && joined[0].records[0].slot == 1 && joined[0].records[0].device == 1,"action matching includes context, device and alternate slot");
            require(joined[0].binding.bRequired && joined[0].binding.MainBinding.aButtonName[0] == "glyph","native required flag and glyph data preserved");
            require(policy.filter(joined,"", "all",162).length == 1 && policy.filter(joined,"", "all",77).length == 0,"modifier identity is numeric, never inferred from localized strings");
            joined = policy.join([native],[],[],function(row:Object):String { return "Missing"; });
            require(!joined[0].available && !joined[0].editable,"missing numeric data is unavailable, not editable unbound");
            native.bGamepadEntry = true;
            native.MainBinding = {aPCKeyName:[],aButtonName:["Xenon_L1","Xenon_Y"]};
            joined = policy.join([native],[],[
                {action:"same",context:0,device:0,slot:0,key:256,modifier:255},
                {action:"same",context:0,device:2,slot:0,key:32768,modifier:256}],function(row:Object):String { return "Controller"; });
            require(joined[0].records.length == 1 && joined[0].gamepad && joined[0].editable, "controller action joins only its own device family");
            require(policy.filter(joined,"shoulder", "all",256).length == 1 && policy.filter(joined,"triangle", "all",32768).length == 1,
                "controller names and both chord members are searchable");
            require(!policy.includes(joined[0].records[0],256), "controller modifier cannot match a keyboard filter");
        }
        private function findInput(container:DisplayObjectContainer):TextField
        {
            if (!container.visible) return null;
            for (var i:int = 0; i < container.numChildren; ++i) {
                var child:DisplayObject = container.getChildAt(i);
                if (child is TextField && TextField(child).type == "input") return child as TextField;
                if (child is DisplayObjectContainer) {
                    var result:TextField = findInput(child as DisplayObjectContainer); if (result) return result;
                }
            }
            return null;
        }
        private function findNamed(container:DisplayObjectContainer, name:String):DisplayObject
        {
            for (var i:int = 0; i < container.numChildren; ++i) {
                var child:DisplayObject = container.getChildAt(i);
                if (child.name == name) return child;
                if (child is DisplayObjectContainer) {
                    var result:DisplayObject = findNamed(child as DisplayObjectContainer, name); if (result) return result;
                }
            }
            return null;
        }
        private function buttonFill(button:DisplayObject):uint
        {
            var pixel:BitmapData = new BitmapData(1,1,false,0);
            pixel.draw(button,new Matrix(1,0,0,1,-8,-8));
            var color:uint = pixel.getPixel(0,0); pixel.dispose(); return color;
        }
        private function issueControlMap(controller:int, accept:String):void
        {
            var manager:Object = menu.loaderInfo.applicationDomain.getDefinition("Shared.AS3.Data.BSUIDataManager");
            var control:Object = manager.GetDataFromClient("ControlMapData");
            if (!originalControlMap) originalControlMap = {controller:control.data.uiController, events:control.data.vMappedEvents};
            var mappings:Array = [];
            for each (var mapping:Object in originalControlMap.events) mappings.push(mapping.strUserEventName == "Accept" ?
                {strUserEventName:"Accept",strButtonName:accept,aButtonName:[accept],sContextName:"BasicMenuNav"} : mapping);
            control.data.uiController = controller < 0 ? originalControlMap.controller : controller;
            control.data.vMappedEvents = controller < 0 ? originalControlMap.events : mappings;
            control.DispatchChange();
        }
        private function capture(name:String, area:Rectangle = null, scale:Number = 2 / 3):void
        {
            var style:Object = menu.loaderInfo.applicationDomain.getDefinition("MenuStyle");
            var status:TextField;
            var buttons:DisplayObject;
            for (var childIndex:int = 0; childIndex < menu.numChildren; ++childIndex) {
                var child:DisplayObject = menu.getChildAt(childIndex);
                if (child is TextField && child.x == style.LEFT && child.y > style.LIST_BOTTOM) status = child as TextField;
                if ("RefreshButtons" in child) buttons = child;
            }
            require(status != null && buttons != null, "footer present: " + name);
            if (status && status.text.length) {
                var bounds:Rectangle = status.getBounds(menu);
                require(status.textHeight + 4 <= status.height && bounds.bottom <= 1080,
                    "footer status fits on screen: " + name);
                require(!bounds.intersects(buttons.getBounds(menu)),
                    "footer status does not overlap button hints: " + name);
            }
            if (!captures) return;
            if (!area) area = new Rectangle(0,0,1920,1080);
            var width:int = Math.ceil(area.width * scale), height:int = Math.ceil(area.height * scale);
            var bitmap:BitmapData = new BitmapData(width, height, false, 0x08151C);
            bitmap.draw(menu, new Matrix(scale, 0, 0, scale,-area.x * scale,-area.y * scale));
            var pixels:ByteArray = new ByteArray();
            for (var y:int = 0; y < height; ++y) {
                pixels.writeByte(0);
                for (var x:int = 0; x < width; ++x) pixels.writeUnsignedInt((bitmap.getPixel(x, y) << 8) | 255);
            }
            bitmap.dispose(); pixels.compress();
            var png:ByteArray = new ByteArray(); png.writeUnsignedInt(0x89504E47); png.writeUnsignedInt(0x0D0A1A0A);
            var header:ByteArray = new ByteArray(); header.writeUnsignedInt(width); header.writeUnsignedInt(height);
            header.writeByte(8); header.writeByte(6); header.writeByte(0); header.writeByte(0); header.writeByte(0);
            chunk(png, "IHDR", header); chunk(png, "IDAT", pixels); chunk(png, "IEND", new ByteArray());
            var alphabet:String = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            var encoded:String = "";
            for (var i:int = 0; i < png.length; i += 3) {
                var bits:uint = (png[i] << 16) | ((i + 1 < png.length ? png[i + 1] : 0) << 8) | (i + 2 < png.length ? png[i + 2] : 0);
                encoded += alphabet.charAt((bits >> 18) & 63) + alphabet.charAt((bits >> 12) & 63) +
                    (i + 1 < png.length ? alphabet.charAt((bits >> 6) & 63) : "=") + (i + 2 < png.length ? alphabet.charAt(bits & 63) : "=");
            }
            trace("[preview-png:" + name + "] " + encoded);
        }
        private function chunk(png:ByteArray, type:String, data:ByteArray):void
        {
            png.writeUnsignedInt(data.length);
            var start:uint = png.position; png.writeUTFBytes(type); png.writeBytes(data);
            var crc:uint = 0xFFFFFFFF;
            for (var i:uint = start; i < png.position; ++i) {
                crc ^= png[i];
                for (var bit:int = 0; bit < 8; ++bit) crc = (crc >>> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
            png.writeUnsignedInt(crc ^ 0xFFFFFFFF);
        }
    }
}
