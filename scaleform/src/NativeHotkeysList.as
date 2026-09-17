package
{
    import flash.display.DisplayObject;
    import flash.events.Event;
    import flash.events.MouseEvent;
    import flash.geom.Rectangle;

    // The authored ControlsList, its rows and conflict popup come from SettingsPanel.swf.
    public final class NativeHotkeysList
    {
        public var list:Object;
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

        public function NativeHotkeysList(create:Function, definition:Function, code:Object, notify:Function)
        {
            bridge = code; changed = notify;
            dataManager = definition("Shared.AS3.Data.BSUIDataManager");
            global = definition("Shared.GlobalFunc");
            list = create("ControlsList");
            // ControlsList's first child is its authored timeline mask, separate from Border_mc.
            var viewport:DisplayObject = list.getChildAt(0) as DisplayObject;
            viewport.x = 0; viewport.y = 0;
            viewport.width = MenuStyle.LIST_WIDTH; viewport.height = MenuStyle.LIST_HEIGHT;
            popup = create("RemapConfirmation");
            popup.PopulateButtonBar(1, 38); popup.active = false;
            dataManager.Subscribe("ControlBindingsData", bindingsChanged);
            dataManager.Subscribe("RemapErrorData", remapError);
            dataManager.Subscribe("RemapConfirmationData", confirmationChanged);
            dataManager.Subscribe("FireForgetEventData", saved);
            dataManager.addEventListener("SettingsPanel_RemapConfirmed", remapConfirmed);
            list.addEventListener(MouseEvent.CLICK, beforeBindingClick, true, 100);
        }

        public function open():void
        {
            if (openRequested) return;
            openRequested = true;
            dataManager.dispatchEvent(new Event("SettingsPanel_OpenSettings", true));
            // Vanilla's CONTROL_MAPPINGS_CATEGORY (separate from general control options).
            dataManager.dispatchCustomEvent("SettingsPanel_OpenCategory", {categoryID:4});
        }

        public function populate(rows:Array, labels:Array):void
        {
            definitions = labels;
            var data:Array = [];
            for each (var row:Object in rows) {
                var native:Object = null;
                for each (var entry:Object in entries) {
                    if (!entry.bIsDivider && entry.uContextID == 0 && entry.sInputName == row.action) {
                        native = entry; break;
                    }
                }
                row.editable = Boolean(native && !native.bReadOnly && !native.bGamepadEntry);
                row.value = native ? native.MainBinding.aPCKeyName.join(" + ") : "";
                row.alternate = native ? native.AltBinding.aPCKeyName.join(" + ") : "";
                if (!native) row.hint = "This action is not available in the current native Controls list.";
                if (native) {
                    // Keep the data model's slot/glyph data intact; attach OSF presentation metadata.
                    var item:Object = {};
                    for (var key:String in native) item[key] = native[key];
                    item.row = row; data.push(item);
                } else {
                    data.push({row:row, sInputName:row.action, uContextID:0, sContextName:"MainGameplay",
                        bReadOnly:true, bIsDivider:false, bRequired:false, bGamepadEntry:false,
                        MainBinding:{aButtonName:[], aPCKeyName:[]}, AltBinding:{aButtonName:[], aPCKeyName:[]}});
                }
            }
            list.InitializeEntries(data);
            list.EnableAltBindings(showAlternate);
        }

        public function decorate():void
        {
            for (var i:int = 0; i < list.totalEntryClips; ++i) {
                var clip:Object = list.GetClipByIndex(i);
                if (!clip || clip.itemIndex < 0) continue;
                var item:Object = list.GetDataForEntry(clip.itemIndex);
                if (!item) continue;
                // Vanilla builds translation keys from the action ID; OSF schemas supply the label.
                global.SetText(clip.Text_mc.textField, item.row.title);
                clip.AltBinding_mc.visible = showAlternate;
                // Measure authored cell bounds so normal and large-text variants fit the viewport.
                var right:Number = MenuStyle.LIST_WIDTH - clip.x - 24;
                var bounds:Rectangle;
                if (showAlternate) {
                    bounds = clip.AltBinding_mc.getBounds(clip);
                    clip.AltBinding_mc.x += right - bounds.right;
                    right -= bounds.width + 24;
                }
                bounds = clip.MainBinding_mc.getBounds(clip);
                clip.MainBinding_mc.x += right - bounds.right;
                clip.Text_mc.scaleX = 1;
                clip.Text_mc.textField.width = Math.max(0, right - bounds.width - 24 - clip.Text_mc.x - clip.Text_mc.textField.x);
                clip.Border_mc.width = MenuStyle.LIST_WIDTH;
                clip.Fill_mc.width = MenuStyle.LIST_WIDTH;
            }
        }

        private function begin():Boolean
        {
            if (busy || saving || !list.selectedEntry || !list.selectedEntry.row.editable) return false;
            if (!bridge.beginNativeBinding()) { changed("Could not start key capture.", false); return false; }
            busy = true; seenRemapping = false; cancelled = false;
            list.disableInput = true; list.disableSelection = true;
            changed("Press a key. Escape cancels.", false);
            return true;
        }

        private function beforeBindingClick(event:MouseEvent):void
        {
            var target:DisplayObject = event.target as DisplayObject;
            while (target && target != list) {
                if (target.name == "MainBinding_mc" || target.name == "AltBinding_mc") {
                    if (!begin()) event.stopImmediatePropagation();
                    return;
                }
                target = target.parent;
            }
        }

        public function press():void
        {
            if (!begin()) return;
            var clip:Object = list.GetClipByIndex(list.selectedClipIndex);
            if (clip.activePriority == 2) clip.SetActiveBinding(0);
            clip.onEntryPressed();
        }

        public function get canClear():Boolean
        {
            if (busy || saving || !list.selectedEntry || !list.selectedEntry.row.editable) return false;
            var binding:Object = list.activePriority == 0 ? list.selectedEntry.MainBinding :
                list.activePriority == 1 ? list.selectedEntry.AltBinding : null;
            return Boolean(binding && (binding.aPCKeyName.length || binding.aButtonName.length));
        }

        public function clearBinding():void
        {
            if (!canClear) return;
            clearing = true; saving = true;
            changed("Clearing binding...", false);
            // The native operation accepts either priority; the stock row's delete
            // method restricts it to alternate bindings before dispatching this event.
            dataManager.dispatchCustomEvent("SettingsPanel_ClearBinding", {
                name:list.selectedEntry.sInputName, keyPriority:list.activePriority,
                contextID:list.selectedEntry.uContextID
            });
        }

        public function cancel():void
        {
            if (!busy) return;
            cancelled = true;
            bridge.endNativeBinding(true);
            finish();
            changed("Binding unchanged.", true);
        }

        private function finish():void
        {
            busy = false; seenRemapping = false;
            popup.active = false;
            list.ClearEntryListenState();
            list.disableInput = false; list.disableSelection = false;
            bridge.endNativeBinding(false);
        }

        private function bindingsChanged(event:Event):void
        {
            var data:Object = Object(event).data;
            entries = data.aInputSettingsList as Array || [];
            showAlternate = Boolean(data.bShowSecondaryBindings);
            if (busy && data.bRemappingControl) seenRemapping = true;
            if (busy && seenRemapping && !data.bRemappingControl) {
                finish();
                if (!cancelled) save();
                else changed("Binding unchanged.", true);
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
            changed("Saving binding...", false);
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
                changed("A required game binding is missing. Restore it in Controls before saving.", true);
            } else if (global.HasFireForgetEvent(data, "SettingsDataModel_ControlsSaved")) {
                saving = false; changed("Binding saved.", true);
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
            list.removeEventListener(MouseEvent.CLICK, beforeBindingClick, true);
        }
    }
}
