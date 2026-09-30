package
{
    import flash.display.DisplayObject;
    import flash.display.MovieClip;
    import flash.geom.ColorTransform;
    import flash.utils.Dictionary;

    // Owns data replacement and presentation of the vanilla list's pooled clips.
    public final class SettingsList
    {
        private static const NORMAL:ColorTransform = new ColorTransform();
        // Selected controls draw in ink on the highlighted row.
        private static const SELECTED:ColorTransform = new ColorTransform(0, 0, 0, 1,
            MenuStyle.INK >> 16 & 0xFF, MenuStyle.INK >> 8 & 0xFF, MenuStyle.INK & 0xFF, 0);
        private var list:Object;
        private var views:Dictionary = new Dictionary(true);

        public function SettingsList(control:Object) { list = control; }

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
                               focused:Boolean, hoverIndex:int):void
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
                    view.dragging != sliderDragging || view.hovered != hovered || view.capturing != capturing) {
                    view.row = row; view.value = row.value; view.frame = clip.currentFrame;
                    view.selected = selected; view.modList = modList; view.height = height; view.dragging = sliderDragging;
                    view.hovered = hovered; view.capturing = capturing;
                    clip.setChildIndex(view.content as DisplayObject, 0);
                    var stepper:Object = Object(clip).LargeStepper_mc;
                    for (var child:int = 0; child < clip.numChildren; ++child) {
                        var display:DisplayObject = clip.getChildAt(child);
                        display.visible = display == view.content || display == binding ||
                            (showSlider && display == slider) || (showStepper && display == stepper);
                    }
                    clip.transform.colorTransform = NORMAL;
                    clip.mouseChildren = showSlider || showStepper || binding != null;
                    clip.mouseEnabled = row.type != "section";
                    var controlX:Number = MenuStyle.LIST_WIDTH - 476;
                    if (showSlider) {
                        slider.x = controlX; slider.y = (height - slider.height) / 2; slider.width = 330;
                        slider.maxValue = NumericSetting.steps(row);
                        slider.disableRounding = false; slider.mouseWheelValueChange = 1;
                        if (!sliderDragging) slider.value = NumericSetting.position(row);
                        slider.transform.colorTransform = selected ? SELECTED : NORMAL;
                    }
                    if (showStepper) {
                        stepper.textField.visible = false;
                        stepper.x = controlX; stepper.width = 450;
                        stepper.y = (height - stepper.height) / 2;
                        stepper.transform.colorTransform = selected ? SELECTED : NORMAL;
                    }
                    var border:MovieClip = Object(clip).Border_mc;
                    border.x = 0; border.y = 0; border.width = issue ? IssueStyle.LIST_WIDTH : MenuStyle.LIST_WIDTH;
                    if (border.height != height) { border.height = height; needsLayout = true; }
                    view.content.update(row, selected, modList, height, hovered);
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
