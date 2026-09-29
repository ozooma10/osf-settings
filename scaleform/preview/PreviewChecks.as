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
                    key(221); break;
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
                    require(navFocused() && list.selectedEntry.row.key == "autoAdvance", "Back returns to the sidebar and keeps the page");
                    userEvent("Right"); list.selectedIndex = indexOf("restoreView"); break;
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
                    userEvent("Right"); list.selectedIndex = indexOf("toggleKey"); userEvent("Accept"); break;
                case 13:
                    require(list.disableInput && Object(menu).BGSCodeObj.pollKeyCapture().state == "waiting", "key capture disables list input");
                    capture("key-waiting");
                    Object(menu).BGSCodeObj.previewKey(116, true); break;
                case 14:
                    userEvent("Accept");
                    require(list.selectedEntry.row.value == 115, "held candidate cannot be confirmed");
                    findNamed(menu, "Camera").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    require(list.selectedEntry.row.key == "toggleKey", "capture blocks tab changes");
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
                    require(list.entryCount == 40,"MainGameplay native rows include both sources and unbound actions");
                    require(list.selectedEntry.row.action == "Jump","binding results start at the first native action");
                    require(!findNamed(menu,"clearBindingFilters").visible && !findNamed(menu,"bindingKeyFilter").visible,"filter controls stay hidden without a filter");
                    capture("keybindings");
                    findNamed(menu,"key_77").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 36:
                    require(list.entryCount == 3,"key filter includes plain and chord bindings");
                    require(findNamed(menu,"bindingKeyFilter").visible && findNamed(menu,"clearBindingFilters").visible,"key filter shows its chip and clear control");
                    findNamed(menu,"key_77").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 37:
                    require(list.entryCount == 40 && !findNamed(menu,"clearBindingFilters").visible,"clicking selected key clears the filter");
                    findNamed(menu,"key_162").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 38:
                    require(list.entryCount == 1 && list.selectedEntry.row.action == "preview/chord","modifier filter finds chord");
                    capture("keybindings-chord");
                    findNamed(menu,"bindingKeyFilter").dispatchEvent(new MouseEvent(MouseEvent.CLICK));
                    require(list.entryCount == 40 && !findNamed(menu,"bindingKeyFilter").visible,"key chip clears the key filter");
                    var search:TextField = searchField();
                    menu.stage.focus = search;
                    search.text = "alternate"; search.dispatchEvent(new Event(Event.CHANGE)); break;
                case 39:
                    require(list.entryCount == 1,"two slots of one action appear once");
                    require(menu.stage.focus == searchField(),"filter refresh preserves search focus");
                    key(221); key(Keyboard.B); key(Keyboard.X); userEvent("Accept"); userEvent("RShoulder");
                    require(list.entryCount == 1 && list.selectedEntry.row.action == "preview/alternate","search suppresses native and raw shortcuts");
                    var input:TextField = searchField();
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
                    require(list.entryCount == 32,"Game source filter");
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
                    require(list.selectedEntry.row.action == "Fixture27","last native action is reachable");
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
                    require(list.entryCount == 40,"current snapshot replaces loading state");
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
                    findNamed(menu,"issues").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 53:
                    require(list.entryCount == 4, "issues fixture has four reports");
                    requireGlyphs("Issues: 4");
                    capture("issues");
                    findNamed(menu,"mods").dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
                case 54:
                    for (var childIndex:int = 0; childIndex < menu.numChildren; ++childIndex) {
                        var candidate:Object = menu.getChildAt(childIndex);
                        if ("shelfHeight" in candidate && "toggleExpanded" in candidate) launcher = candidate;
                    }
                    require(launcher && launcher.visible && !launcher.expanded && list.visible, "Home combines interfaces and mod settings");
                    require(findNamed(menu,"launcher") == null, "separate Launcher tab removed");
                    // The shelf holds two rows; its last card links to the rest of the ten interfaces.
                    var moreCard:String = "launcher_" + (launcher.columns * 2 - 1);
                    require(findNamed(menu,moreCard) != null && findNamed(menu,"launcher_" + launcher.columns * 2) == null &&
                        Object(findNamed(menu,moreCard)).row.more && Object(findNamed(menu,moreCard)).row.badge == "+" + (11 - launcher.columns * 2),
                        "Home fills two shelf rows and adds a more card");
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
                    launcher.navigate("Down"); userEvent("Down"); break;
                case 56:
                    require(!launcher.focused && !list.disableInput, "Down from the shelf returns to mod settings");
                    findNamed(menu,"launcher_" + (launcher.columns * 2 - 1)).dispatchEvent(new MouseEvent(MouseEvent.CLICK)); break;
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
                            key:"openMenu", action:"osfsettings/openMenu", registered:true, title:"Open mod settings", hint:"", defaultName:"F10"}];
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
                    userEvent("Right"); list.selectedIndex = advanced + 1; userEvent("YButton");
                    require(list.selectedEntry.row.actionState == "Run", "reset leaves action controls unchanged");
                    userEvent("Accept"); break;
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
            if (step == 35 || step == 48) return list.entryCount == 40;
            if (step == 61) {
                var first:Object = findNamed(menu,"launcher_0");
                return first && first.row.title == "Ship Planner";
            }
            if (step == 63) return !launcher.visible;
            if (step == 65) return findNamed(menu,"homeEmpty").visible;
            // Locked alone is left over from the empty state; wait for the interfaces to arrive.
            if (step == 68) return launcher.locked && launcher.visible;
            if (step == 70) return launcher.visible && !launcher.locked;
            return true;
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
        private function requireGlyphs(text:String):void
        {
            var field:TextField;
            for (var i:int = 0; i < menu.numChildren; ++i) {
                var candidate:TextField = menu.getChildAt(i) as TextField;
                if (candidate && candidate.text == text) { field = candidate; break; }
            }
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
            var controller:Object = {sInputName:"same",uContextID:0,bGamepadEntry:true,bIsDivider:false,bReadOnly:false,bRequired:false,
                MainBinding:{aPCKeyName:[],aButtonName:["A"]},AltBinding:{aPCKeyName:[],aButtonName:[]}};
            joined = policy.join([native,controller],[],[
                {action:"same",context:0,device:0,slot:0,key:64,modifier:255},
                {action:"same",context:0,device:2,slot:0,key:4096,modifier:64}],function(row:Object):String { return "Same action"; });
            require(joined.length == 2 && joined[0].identity != joined[1].identity && joined[0].records.length == 1 && joined[1].records.length == 1,
                "keyboard and controller rows of one action retain separate numeric bindings");
            require(joined[1].value == "A" && joined[1].editable && joined[1].binding.MainBinding.aButtonName[0] == "A",
                "controller binding remains editable and preserves the native glyph");
            require(policy.filter(joined,"", "all",64).length == 1 && policy.filter(joined,"", "all",64,true).length == 1 &&
                policy.filter([joined[1]],"", "all",64).length == 0,"controller modifiers cannot appear as keyboard keys");
            require(policy.filter([joined[1]],"", "all",4096,true).length == 1,"controller face button filter accepts native IDs above 255");
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
        private function capture(name:String):void
        {
            if (!captures) return;
            var bitmap:BitmapData = new BitmapData(1280, 720, false, 0x08151C);
            bitmap.draw(menu, new Matrix(2 / 3, 0, 0, 2 / 3));
            var pixels:ByteArray = new ByteArray();
            for (var y:int = 0; y < 720; ++y) {
                pixels.writeByte(0);
                for (var x:int = 0; x < 1280; ++x) pixels.writeUnsignedInt((bitmap.getPixel(x, y) << 8) | 255);
            }
            bitmap.dispose(); pixels.compress();
            var png:ByteArray = new ByteArray(); png.writeUnsignedInt(0x89504E47); png.writeUnsignedInt(0x0D0A1A0A);
            var header:ByteArray = new ByteArray(); header.writeUnsignedInt(1280); header.writeUnsignedInt(720);
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
