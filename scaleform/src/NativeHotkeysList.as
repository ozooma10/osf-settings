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
                    if (!entry.bIsDivider && !entry.bGamepadEntry && entry.uContextID == 0 && entry.sInputName == row.action) {
                        native = entry; break;
                    }
                }
                row.editable = Boolean(native && !native.bReadOnly && !native.bGamepadEntry);
                row.value = native ? native.MainBinding.aPCKeyName.join(" + ") : "";
                row.alternate = native ? native.AltBinding.aPCKeyName.join(" + ") : "";
                if (!native) row.hint = Localization.text("bindings.unavailableAction");
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

        public function decorate(host:MovieClip, row:Object, selected:Boolean):DisplayObject
        {
            var view:Object = views[host];
            if (row.type != "hotkey") {
                if (view) { view.clip.ClearActiveBinding(); view.row = null; }
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
            if (view.row != row) {
                if (!view.row || view.row.action != row.action) clip.ClearActiveBinding();
                clip.SetEntryText(row.binding); view.row = row;
            }
            clip.itemIndex = Object(host).itemIndex;
            if (!busy) {
                if (selected) {
                    if (!clip.selected) clip.onRollover();
                    if (clip.activePriority == 2 || !(showAlternate || fullPage)) clip.SetActiveBinding(0);
                } else if (clip.selected || clip.activePriority != 2) clip.onRollout();
            }
            // The outer SettingsRow supplies the label, background and row hit area.
            for (var i:int = 0; i < clip.numChildren; ++i) {
                var child:DisplayObject = clip.getChildAt(i);
                child.visible = child == clip.MainBinding_mc || (showAlternate || fullPage) && child == clip.AltBinding_mc;
            }
            var right:Number = MenuStyle.LIST_WIDTH - 24;
            for each (var cell:Object in showAlternate || fullPage ? [clip.AltBinding_mc, clip.MainBinding_mc] : [clip.MainBinding_mc]) {
                CONFIG::preview { bridge.previewBindingCell(cell); }
                cell.scaleX = cell.scaleY = 1;
                var bounds:Rectangle = cell.getBounds(clip);
                cell.scaleX = cell.scaleY = Math.min(1, 224 / bounds.width, (MenuStyle.ROW_HEIGHT - 12) / bounds.height);
                bounds = cell.getBounds(clip);
                cell.x += right - bounds.right;
                cell.y += (MenuStyle.ROW_HEIGHT - bounds.height) / 2 - bounds.top;
                right -= fullPage ? 238 : bounds.width + 24;
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
            if (!clip || busy || saving || !(showAlternate || fullPage)) return;
            if (clip.activePriority == 2) clip.SetActiveBinding(0);
            else clip.onKeyDownHandler(event);
        }

        private function begin():Boolean
        {
            if (busy || saving || !currentClip || !list.selectedEntry.row.editable) return false;
            if (!bridge.beginNativeBinding()) { changed(Localization.text("errors.capture"), false); return false; }
            busy = true; seenRemapping = false; cancelled = false;
            list.disableInput = true; list.disableSelection = true;
            changed(Localization.text("bindings.capture"), false);
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
                currentClip.activePriority == 1 ? list.selectedEntry.row.binding.AltBinding : null;
            return Boolean(binding && (binding.aPCKeyName.length || binding.aButtonName.length));
        }

        public function clearBinding():void
        {
            if (!canClear) return;
            clearing = true; saving = true;
            changed(Localization.text("bindings.clearing"), false);
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
            changed(Localization.text("bindings.unchanged"), true);
        }

        private function finish():void
        {
            busy = false; seenRemapping = false;
            popup.active = false;
            if (currentClip) currentClip.ClearListenState();
            list.disableInput = false; list.disableSelection = false;
            bridge.endNativeBinding(false);
        }

        private function bindingsChanged(event:Event):void
        {
            var data:Object = Object(event).data;
            entries = data.aInputSettingsList as Array || [];
            ready = true; ++revision;
            showAlternate = Boolean(data.bShowSecondaryBindings);
            if (busy && data.bRemappingControl) seenRemapping = true;
            if (busy && seenRemapping && !data.bRemappingControl) {
                finish();
                if (!cancelled) save();
                else changed(Localization.text("bindings.unchanged"), true);
            } else if (clearing) {
                clearing = false; save();
            }
            changed("", !busy);
        }

        private function remapConfirmed(event:Event):void
        {
            if (busy) cancelled = !Boolean(Object(event).params.confirmed);
        }

        private function save():void
        {
            saving = true;
            changed(Localization.text("bindings.saving"), false);
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
                changed(Localization.text("bindings.requiredMissing"), true);
            } else if (global.HasFireForgetEvent(data, "SettingsDataModel_ControlsSaved")) {
                saving = false; changed(Localization.text("bindings.saved"), true);
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
