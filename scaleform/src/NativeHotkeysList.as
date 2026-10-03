package
{
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.events.Event;
    import flash.events.KeyboardEvent;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;
    import flash.utils.Dictionary;

    // Adapts vanilla binding rows to the same scrolling list as ordinary settings.
    public final class NativeHotkeysList
    {
        private var list:Object;
        private var create:Function;
        private var views:Dictionary = new Dictionary(true);
        public var popup:Object;
        public var busy:Boolean = false;
        public var saving:Boolean = false;
        private var dataManager:Object;
        private var global:Object;
        private var bridge:Object;
        private var changed:Function;
        private var entries:Array = [];
        private var definitions:Array = [];
        private var seenRemapping:Boolean = false;
        private var cancelled:Boolean = false;
        private var clearing:Boolean = false;
        private var openRequested:Boolean = false;
        private var showAlternate:Boolean = false;
        private var pending:Object;
        public var gamepad:Boolean = false;
        public function get secondary():Boolean { return showAlternate; }
        public var revision:uint = 0;
        public var ready:Boolean = false;
        public var fullPage:Boolean = false;
        private var labelEntry:Object;

        public function get bindings():Array { return entries; }
        public function title(entry:Object):String
        {
            if (!labelEntry) labelEntry = create("Shared.Components.SystemPanels.SettingsControlListEntry");
            labelEntry.SetEntryText(entry);
            CONFIG::preview { return bridge.previewBindingTitle(entry); }
            return String(labelEntry.Text_mc.textField.text);
        }

        public function NativeHotkeysList(sharedList:Object, factory:Function, definition:Function, code:Object, notify:Function)
        {
            list = sharedList; create = factory; bridge = code; changed = notify;
            dataManager = definition("Shared.AS3.Data.BSUIDataManager");
            global = definition("Shared.GlobalFunc");
            popup = create("RemapConfirmation");
            // The popup's authored button-bar anchor is at the center of its box.
            popup.PopulateButtonBar(definition("Shared.Components.ButtonControls.ButtonBar.ButtonBar").JUSTIFY_CENTER, 38);
            popup.active = false;
            dataManager.Subscribe("ControlBindingsData", bindingsChanged);
            dataManager.Subscribe("RemapErrorData", remapError);
            dataManager.Subscribe("RemapConfirmationData", confirmationChanged);
            dataManager.Subscribe("FireForgetEventData", saved);
            dataManager.addEventListener("SettingsPanel_RemapConfirmed", remapConfirmed);
            for each (var type:String in [MouseEvent.MOUSE_DOWN, MouseEvent.CLICK, MouseEvent.MOUSE_OVER, MouseEvent.MOUSE_OUT, MouseEvent.MOUSE_WHEEL])
                list.addEventListener(type, blockMouse, true, 100);
        }

        public function open():void
        {
            if (openRequested) return;
            openRequested = true;
            dataManager.dispatchEvent(new Event("SettingsPanel_OpenSettings", true));
            // Vanilla's CONTROL_MAPPINGS_CATEGORY (separate from general control options).
            dataManager.dispatchCustomEvent("SettingsPanel_OpenCategory", {categoryID:4});
        }

        public function populate(rows:Array):void
        {
            definitions = rows;
            for each (var row:Object in rows) {
                if (row.type != "hotkey") continue;
                var native:Object = null;
                for each (var entry:Object in entries) {
                    if (!entry.bIsDivider && entry.uContextID == 0 && entry.sInputName == row.action) {
                        native = entry; break;
                    }
                }
                row.editable = Boolean(native && !native.bReadOnly);
                row.value = native ? KeybindingsData.bindingText(native.MainBinding) : "";
                row.alternate = native && showAlternate ? KeybindingsData.bindingText(native.AltBinding) : "";
                if (!native) row.hint = tr("bindings.unavailableAction");
                if (native) {
                    // Keep the data model's slot/glyph data intact.
                    var item:Object = {};
                    for (var key:String in native) item[key] = native[key];
                    row.binding = item;
                } else {
                    row.binding = {sInputName:row.action, uContextID:0, sContextName:"MainGameplay",
                        bReadOnly:true, bIsDivider:false, bRequired:false, bGamepadEntry:false,
                        MainBinding:{aButtonName:[], aPCKeyName:[]}, AltBinding:{aButtonName:[], aPCKeyName:[]}};
                }
            }
        }

        public function decorate(host:MovieClip, row:Object, selected:Boolean, height:Number):DisplayObject
        {
            var view:Object = views[host];
            if (row.type != "hotkey") {
                if (view && view.row) { view.clip.ClearActiveBinding(); view.row = null; }
                return null;
            }
            if (!view) {
                var native:Object = create("Shared.Components.SystemPanels.SettingsControlListEntry");
                native.mouseEnabled = false;
                native.addEventListener(MouseEvent.CLICK, beforeBindingClick, true, 100);
                host.addChild(native as MovieClip);
                view = {clip:native, row:null}; views[host] = view;
            }
            var clip:Object = view.clip;
            var bindingChanged:Boolean = view.row != row || view.binding != row.binding;
            if (bindingChanged) {
                if (!view.row || view.row.action != row.action || view.binding.bGamepadEntry != row.binding.bGamepadEntry) clip.ClearActiveBinding();
                clip.SetEntryText(row.binding); view.row = row; view.binding = row.binding;
            }
            clip.itemIndex = Object(host).itemIndex;
            if (!busy) {
                if (selected) {
                    if (!clip.selected) clip.onRollover();
                    if (clip.activePriority == 2 || !showAlternate && clip.activePriority != 0) clip.SetActiveBinding(0);
                } else if (clip.selected || clip.activePriority != 2) clip.onRollout();
            }
            var alternate:Boolean = showAlternate;
            // Native rollover/listening frames can change bounds; idle cells cannot.
            if (!bindingChanged && view.height == height && view.alternate == alternate && view.fullPage == fullPage &&
                view.frame == clip.currentFrame && view.mainFrame == clip.MainBinding_mc.currentFrame && view.altFrame == clip.AltBinding_mc.currentFrame)
                return clip as DisplayObject;
            view.height = height; view.alternate = alternate; view.fullPage = fullPage;
            view.frame = clip.currentFrame; view.mainFrame = clip.MainBinding_mc.currentFrame; view.altFrame = clip.AltBinding_mc.currentFrame;
            // The outer SettingsRow supplies the label, background and row hit area.
            for (var i:int = 0; i < clip.numChildren; ++i) {
                var child:DisplayObject = clip.getChildAt(i);
                child.visible = child == clip.MainBinding_mc || alternate && child == clip.AltBinding_mc;
            }
            var left:Number = MenuStyle.CONTROL_X;
            var width:Number = alternate ? MenuStyle.BINDING_WIDTH : MenuStyle.CONTROL_WIDTH;
            for each (var cell:Object in alternate ? [clip.MainBinding_mc, clip.AltBinding_mc] : [clip.MainBinding_mc]) {
                CONFIG::preview { bridge.previewBindingCell(cell); }
                cell.scaleX = cell.scaleY = 1;
                var bounds:Rectangle = cell.getBounds(clip);
                cell.scaleX = cell.scaleY = Math.min(1, Math.min(224, width) / bounds.width, (height - 12) / bounds.height);
                MenuStyle.centerControl(cell as DisplayObject, left, width, height);
                left += width + MenuStyle.BINDING_GAP;
            }
            return clip as DisplayObject;
        }

        private function get currentClip():Object
        {
            var host:Object = list.FindClipForEntry(list.selectedIndex);
            var view:Object = host ? views[host] : null;
            return view && list.selectedEntry && view.row == list.selectedEntry.row ? view.clip : null;
        }
        public function get selectedSlot():int { return currentClip ? currentClip.activePriority : 2; }
        CONFIG::testHarness {
            public function slotRect(slot:int):Rectangle
            {
                if (!currentClip) return new Rectangle();
                return DisplayObject(slot == 0 ? currentClip.MainBinding_mc : currentClip.AltBinding_mc).getBounds(list.stage);
            }
        }

        public function navigate(event:KeyboardEvent):void
        {
            var clip:Object = currentClip;
            if (!clip || busy || saving || !showAlternate) return;
            if (clip.activePriority == 2) clip.SetActiveBinding(0);
            else clip.onKeyDownHandler(event);
        }

        public function userEvent(name:String, pressed:Boolean, event:KeyboardEvent = null):Boolean
        {
            if (event) {
                // Native input has already delivered these keys to the remap receiver.
                event.stopImmediatePropagation();
                if (pressed) event.preventDefault();
                return true;
            }
            if (popup.active) return Boolean(popup.ProcessUserEvent(name, pressed));
            if (pressed && name == "Cancel") cancel();
            return true;
        }

        private function begin():Boolean
        {
            if (busy || saving || !currentClip || !list.selectedEntry.row.editable) return false;
            if (!bridge.beginNativeBinding()) { changed(tr("errors.capture"), false); return false; }
            busy = true; seenRemapping = false; cancelled = false;
            changed(tr("bindings.capture"), false);
            return true;
        }

        private function blockMouse(event:MouseEvent):void
        {
            // Freeze the shared list, including vanilla child controls and slot hover,
            // without changing the active cell's listening state during capture.
            if (busy || saving) { event.stopImmediatePropagation(); event.preventDefault(); }
        }

        private function beforeBindingClick(event:MouseEvent):void
        {
            var target:DisplayObject = event.target as DisplayObject;
            while (target && target != list) {
                if (target.name == "MainBinding_mc" || target.name == "AltBinding_mc") {
                    if (target.name == "AltBinding_mc" && !showAlternate) { event.stopImmediatePropagation(); return; }
                    var clip:Object = event.currentTarget;
                    // Select the clicked slot even if no rollover preceded the click.
                    list.selectedIndex = clip.itemIndex;
                    if (busy || saving) { event.stopImmediatePropagation(); return; }
                    clip.SetActiveBinding(target.name == "AltBinding_mc" ? 1 : 0);
                    if (!begin()) event.stopImmediatePropagation();
                    return;
                }
                target = target.parent;
            }
        }

        public function press():void
        {
            if (!begin()) return;
            var clip:Object = currentClip;
            if (clip.activePriority == 2) clip.SetActiveBinding(0);
            clip.onEntryPressed();
        }

        public function get canClear():Boolean
        {
            if (busy || saving || !currentClip || !list.selectedEntry.row.editable) return false;
            var binding:Object = currentClip.activePriority == 0 ? list.selectedEntry.row.binding.MainBinding :
                showAlternate && currentClip.activePriority == 1 ? list.selectedEntry.row.binding.AltBinding : null;
            return Boolean(binding && (binding.aPCKeyName.length || binding.aButtonName.length));
        }

        public function clearBinding():void
        {
            if (!canClear) return;
            clearing = true; saving = true;
            changed(tr("bindings.clearing"), false);
            // The native operation accepts either priority; the stock row's delete
            // method restricts it to alternate bindings before dispatching this event.
            dataManager.dispatchCustomEvent("SettingsPanel_ClearBinding", {
                name:list.selectedEntry.row.binding.sInputName, keyPriority:currentClip.activePriority,
                contextID:list.selectedEntry.row.binding.uContextID
            });
        }

        public function cancel():void
        {
            if (!busy) return;
            cancelled = true;
            bridge.endNativeBinding(true);
            finish();
            changed(tr("bindings.unchanged"), true);
        }

        private function finish():void
        {
            busy = false; seenRemapping = false;
            popup.active = false;
            if (currentClip) currentClip.ClearListenState();
            bridge.endNativeBinding(false);
        }

        private function bindingsChanged(event:Event):void
        {
            var data:Object = Object(event).data;
            pending = {entries:data.aInputSettingsList as Array || [], secondary:Boolean(data.bShowSecondaryBindings)};
            if (busy && data.bRemappingControl) seenRemapping = true;
            if (busy && seenRemapping && !data.bRemappingControl) {
                finish();
                if (!cancelled) save();
                else changed(tr("bindings.unchanged"), true);
            } else if (clearing) {
                clearing = false; save();
            }
            advance();
            changed("", !busy && !saving);
        }

        public function advance():void
        {
            if (!pending || busy || saving) return;
            entries = pending.entries; showAlternate = pending.secondary; pending = null;
            // Empty publications retain the last device family until rows arrive.
            for each (var entry:Object in entries) if (!entry.bIsDivider) { gamepad = Boolean(entry.bGamepadEntry); break; }
            ready = true; ++revision;
            changed("", true);
        }

        private function remapConfirmed(event:Event):void
        {
            if (busy) cancelled = !Boolean(Object(event).params.confirmed);
        }

        private function save():void
        {
            saving = true;
            changed(tr("bindings.saving"), false);
            dataManager.dispatchEvent(new Event("SettingsPanel_ValidateControls", true));
        }

        private function saved(event:Event):void
        {
            if (!saving) return;
            var data:Object = Object(event).data;
            if (global.HasFireForgetEvent(data, "SettingsDataModel_ControlsValidatedEvent")) {
                dataManager.dispatchEvent(new Event("SettingsPanel_SaveControls", true));
            } else if (global.HasFireForgetEvent(data, "SettingsDataModel_MissingRequiredBindingError")) {
                saving = false;
                changed(tr("bindings.requiredMissing"), true);
            } else if (global.HasFireForgetEvent(data, "SettingsDataModel_ControlsSaved")) {
                saving = false; changed(tr("bindings.saved"), true);
            }
        }

        private function remapError(event:Event):void
        {
            if (busy && Object(event).data.sErrorText) {
                popup.active = false;
                changed(String(Object(event).data.sErrorText), false);
            }
        }

        private function confirmationChanged(event:Event):void
        {
            if (!busy) { popup.active = false; return; }
            var data:Object = Object(event).data;
            for each (var row:Object in definitions) {
                if (row.action == data.sUserEvent) global.SetText(popup.ControlInfo_mc.Label_mc.Text_tf, row.title);
            }
        }

        public function dispose():void
        {
            cancel();
            dataManager.Unsubscribe("ControlBindingsData", bindingsChanged);
            dataManager.Unsubscribe("RemapErrorData", remapError);
            dataManager.Unsubscribe("RemapConfirmationData", confirmationChanged);
            dataManager.Unsubscribe("FireForgetEventData", saved);
            dataManager.removeEventListener("SettingsPanel_RemapConfirmed", remapConfirmed);
            for each (var type:String in [MouseEvent.MOUSE_DOWN, MouseEvent.CLICK, MouseEvent.MOUSE_OVER, MouseEvent.MOUSE_OUT, MouseEvent.MOUSE_WHEEL])
                list.removeEventListener(type, blockMouse, true);
            for each (var view:Object in views) view.clip.removeEventListener(MouseEvent.CLICK, beforeBindingClick, true);
        }
    }
}
