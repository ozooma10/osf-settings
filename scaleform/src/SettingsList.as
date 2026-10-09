package
{
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.geom.ColorTransform;
    import flash.geom.Rectangle;
    import flash.utils.Dictionary;

    // Owns data replacement and presentation of the vanilla list's pooled clips.
    public final class SettingsList
    {
        private static const NORMAL:ColorTransform = new ColorTransform();
        // Selected controls draw in ink on the highlighted row.
        private static const SELECTED:ColorTransform = new ColorTransform(0, 0, 0, 1,
            MenuStyle.INK >> 16 & 0xFF, MenuStyle.INK >> 8 & 0xFF, MenuStyle.INK & 0xFF, 0);
        private var list:Object;
        private var types:Class;
        private var views:Dictionary = new Dictionary(true);

        public function SettingsList(control:Object, entryTypes:Class) { list = control; types = entryTypes; }

        public function resize(width:Number, rowHeight:Number):void
        {
            // Whole rows only, so the last visible row stays above the footer.
            var pitch:Number = rowHeight + MenuStyle.ROW_GAP;
            var height:Number = Math.max(1, Math.floor((MenuStyle.LIST_BOTTOM - list.y + MenuStyle.ROW_GAP) / pitch)) * pitch - MenuStyle.ROW_GAP;
            // Setting borderHeight updates every vanilla clip, even for the same value.
            if (list.borderHeight != height || list.Border_mc.width != width) {
                list.Border_mc.width = width; list.borderHeight = height;
            }
            list.scrollBarHeight = height;
            if (list.ScrollBar) list.ScrollBar.x = width + 14;
            MovieClip(list).getChildByName("EntryHolder_mc").scrollRect = new Rectangle(0, 0, width, height);
        }

        // Convert visible page rows to the vanilla list contract. Sections remain
        // entries so headers scroll with their settings and can be skipped by selection.
        public function entries(rows:Array, settings:Boolean, section:Boolean):Array
        {
            var data:Array = [];
            var headed:Boolean = false, firstGroup:String = null, lastGroup:String = null;
            for each (var row:Object in rows) if (settings) {
                if (firstGroup == null) firstGroup = row.group;
                else if (row.group != firstGroup) headed = true;
            }
            for each (row in rows) {
                if (headed && row.group != lastGroup) {
                    lastGroup = row.group;
                    // Only IDs using "Tab - Section" opt into abbreviated labels.
                    var title:String = String(row.groupTitle), split:int = title.indexOf(" - ");
                    if (section && String(row.group).indexOf(" - ") > 0 && split > 0) title = title.substr(split + 3);
                    data.push({row:{type:"section", title:title, mod:row.mod, id:"@section/" + row.group, group:row.group, editable:false},
                        sText:"", uID:data.length, bDisabled:true, bShowSpinner:false, uCategory:0, bEnabled:false, bSubSetting:false,
                        uType:types.SDT_LINK, sliderData:{fValue:0, sDisplayValue:""}, stepperData:{aStepperOptions:[], uIndex:0}, checkBoxData:{bChecked:false}});
                }
                var slider:Boolean = settings && NumericSetting.isSlider(row);
                var text:String = String(row.title).split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
                // The vanilla entry multiplies fValue by 100; our slider stores integer offsets.
                data.push({row:row, sText:text, uID:data.length, bDisabled:false, bShowSpinner:false,
                    uCategory:0, bEnabled:!settings || row.editable, bSubSetting:false,
                    uType:!settings ? types.SDT_LINK : slider ? types.SDT_SLIDER : row.type == "enum" ? types.SDT_LARGE_STEPPER : row.type == "bool" ? types.SDT_CHECKBOX : types.SDT_LINK,
                    sliderData:{fValue:slider ? NumericSetting.position(row) / 100 : 0, sDisplayValue:NumericSetting.text(row, row.value)},
                    stepperData:{aStepperOptions:row.type == "enum" ? EnumSetting.labels(row) : [], uIndex:row.type == "enum" ? EnumSetting.index(row) : 0}, checkBoxData:{bChecked:row.type == "bool" && row.value}});
            }
            return data;
        }

        // Returns true only when membership/order changed or the page was reset.
        public function setEntries(data:Array, preserve:Boolean):Boolean
        {
            var rebuild:Boolean = !preserve || data.length != list.entryCount;
            for (var i:int = 0; !rebuild && i < data.length; ++i)
                rebuild = !sameRow(list.GetDataForEntry(i).row, data[i].row);
            if (rebuild) {
                list.InitializeEntries(data);
            } else {
                // GetDataForEntry returns the shared raw/visible entry object.
                for (i = 0; i < data.length; ++i) {
                    var entry:Object = list.GetDataForEntry(i);
                    for (var key:String in data[i]) entry[key] = data[i][key];
                }
                for (i = 0; i < list.totalEntryClips; ++i) {
                    var clip:Object = list.GetClipByIndex(i);
                    if (!clip || clip.itemIndex < 0) continue;
                    clip.SetEntryText(list.GetDataForEntry(clip.itemIndex));
                    if (clip.itemIndex == list.selectedIndex) clip.onRollover();
                }
            }
            // Native SetEntryText can reset controls even when it reuses a row.
            for each (var view:Object in views) view.row = null;
            return rebuild;
        }

        private function sameRow(a:Object, b:Object):Boolean
        {
            return a.type == b.type && a.mod == b.mod && a.key == b.key && a.id == b.id && a.identity == b.identity;
        }

        public function get dragging():Boolean
        {
            if (list.scrollbarScrolling) return true;
            for (var i:int = 0; i < list.totalEntryClips; ++i) {
                var clip:Object = list.GetClipByIndex(i);
                if (clip && clip.IsSlider() && clip.Slider_mc.dragging) return true;
            }
            return false;
        }

        public function render(hotkeys:NativeHotkeysList, height:Number, settings:Boolean, bindings:Boolean,
                               focused:Boolean, hoverIndex:int, editingIndex:int = -1):void
        {
            if (!MovieClip(list).visible) return;
            var needsLayout:Boolean = false;
            var modList:Boolean = !settings && !bindings;
            for (var i:int = 0; i < list.totalEntryClips; ++i) {
                var clip:MovieClip = list.GetClipByIndex(i) as MovieClip;
                if (!clip || Object(clip).itemIndex < 0) continue;
                var item:Object = list.GetDataForEntry(Object(clip).itemIndex);
                if (!item) continue;
                var row:Object = item.row;
                var issue:Boolean = row.type == "issue";
                var view:Object = views[clip];
                if (!view || issue != (view.content is IssueRow)) {
                    if (view) clip.removeChild(view.content as DisplayObject);
                    view = {content:issue ? new IssueRow() : new SettingsRow(), row:null};
                    clip.addChild(view.content as DisplayObject); views[clip] = view;
                }
                var selected:Boolean = Object(clip).itemIndex == list.selectedIndex && focused;
                var editing:Boolean = selected && Object(clip).itemIndex == editingIndex;
                var binding:DisplayObject = hotkeys.decorate(clip, row, selected, height);
                var slider:Object = Object(clip).Slider_mc;
                var showSlider:Boolean = settings && NumericSetting.isSlider(row);
                var showStepper:Boolean = settings && row.type == "enum" && row.editable;
                var sliderDragging:Boolean = showSlider && slider.dragging;
                var hovered:Boolean = Object(clip).itemIndex == hoverIndex && !selected;
                var capturing:Boolean = Boolean(row.capturing);
                // Poll only the state vanilla can change under us; stable clips keep their layout.
                if (view.row != row || view.value !== row.value || view.frame != clip.currentFrame ||
                    view.selected != selected || view.modList != modList || view.height != height ||
                    view.dragging != sliderDragging || view.hovered != hovered || view.capturing != capturing || view.editing != editing) {
                    view.row = row; view.value = row.value; view.frame = clip.currentFrame;
                    view.selected = selected; view.modList = modList; view.height = height; view.dragging = sliderDragging;
                    view.hovered = hovered; view.capturing = capturing;
                    view.editing = editing;
                    clip.setChildIndex(view.content as DisplayObject, 0);
                    var stepper:Object = Object(clip).LargeStepper_mc;
                    for (var child:int = 0; child < clip.numChildren; ++child) {
                        var display:DisplayObject = clip.getChildAt(child);
                        display.visible = display == view.content || display == binding ||
                            (showSlider && display == slider) || (showStepper && display == stepper);
                    }
                    clip.transform.colorTransform = NORMAL;
                    clip.mouseChildren = editing && (showSlider || showStepper) || binding != null;
                    clip.mouseEnabled = row.type != "section";
                    if (showSlider) {
                        slider.width = MenuStyle.SLIDER_WIDTH;
                        MenuStyle.centerControl(slider as DisplayObject, MenuStyle.CONTROL_X, MenuStyle.SLIDER_WIDTH, height);
                        slider.maxValue = NumericSetting.steps(row);
                        slider.disableRounding = false; slider.mouseWheelValueChange = 1;
                        if (!sliderDragging) slider.value = NumericSetting.position(row);
                        slider.transform.colorTransform = selected ? SELECTED : NORMAL;
                    }
                    if (showStepper) {
                        stepper.textField.visible = false;
                        stepper.width = MenuStyle.CONTROL_WIDTH;
                        MenuStyle.centerControl(stepper as DisplayObject, MenuStyle.CONTROL_X, MenuStyle.CONTROL_WIDTH, height);
                        stepper.transform.colorTransform = selected ? SELECTED : NORMAL;
                    }
                    var border:MovieClip = Object(clip).Border_mc;
                    border.x = 0; border.y = 0; border.width = issue ? IssueStyle.LIST_WIDTH : MenuStyle.LIST_WIDTH;
                    if (border.height != height) { border.height = height; needsLayout = true; }
                    if (issue) view.content.update(row, selected, modList, height, hovered);
                    else view.content.update(row, selected, modList, height, hovered, editing);
                }
                clip.x = 0; clip.y = (Object(clip).itemIndex - list.scrollPosition) * (height + MenuStyle.ROW_GAP);
            }
            if (needsLayout && !dragging) {
                list.UpdateContainerRect();
                for each (view in views) view.row = null;
            }
        }
    }
}
