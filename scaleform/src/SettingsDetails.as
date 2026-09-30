package
{
    import flash.display.Sprite;
    import flash.events.MouseEvent;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // Owns the selected setting's layout and scroll position.
    public final class SettingsDetails extends Sprite
    {
        private var detailTitle:TextField;
        private var detailLabel:TextField;
        private var detailDivider:Sprite = new Sprite();
        private var detailDecoration:Sprite = new Sprite();
        private var changedLegend:Sprite = new Sprite();
        private var detailHint:TextField;
        private var defaultLabel:TextField;
        private var defaultValue:TextField;
        private var rangeLabel:TextField;
        private var rangeValue:TextField;
        private var detailOptions:Sprite = new Sprite();
        private var selected:Object;
        private var selectedValue:*;
        private var wasEditing:Boolean;
        private var previousTop:Number = NaN;
        public var editorTop:Number;
        public function SettingsDetails()
        {
            detailDecoration.mouseEnabled = detailDecoration.mouseChildren = false; addChild(detailDecoration);
            detailDivider.mouseEnabled = false; addChild(detailDivider);
            detailDivider.graphics.lineStyle(1, MenuStyle.LINE);
            detailDivider.graphics.moveTo(MenuStyle.DETAIL_X, 0); detailDivider.graphics.lineTo(MenuStyle.RIGHT, 0);
            var detailWidth:Number = MenuStyle.DETAIL_WIDTH;
            detailLabel = label(tr("menu.selectedSetting"), MenuStyle.DETAIL_X, 0, detailWidth, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            detailTitle = label("", MenuStyle.DETAIL_X, 0, detailWidth, 80, MenuStyle.DETAIL_TITLE_SIZE);
            detailTitle.multiline = true; detailTitle.wordWrap = true;
            detailHint = label("", MenuStyle.DETAIL_X, 0, detailWidth, 120, MenuStyle.DETAIL_BODY_SIZE, MenuStyle.MUTED);
            detailHint.multiline = true; detailHint.wordWrap = true; detailHint.mouseEnabled = true;
            var descriptionFormat:TextFormat = detailHint.defaultTextFormat;
            descriptionFormat.leading = CONFIG::largeText ? 8 : 6; detailHint.defaultTextFormat = descriptionFormat;
            detailHint.addEventListener(MouseEvent.MOUSE_WHEEL, scrollDescription);
            defaultLabel = label(tr("menu.default"), MenuStyle.DETAIL_X, 0, 240, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            defaultValue = label("", MenuStyle.RIGHT - 300, 0, 300, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true); alignRight(defaultValue);
            rangeLabel = label(tr("menu.range"), MenuStyle.DETAIL_X, 0, 240, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true);
            rangeValue = label("", MenuStyle.RIGHT - 300, 0, 300, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true); alignRight(rangeValue);
            detailOptions.x = MenuStyle.DETAIL_X; detailOptions.mouseEnabled = detailOptions.mouseChildren = false; addChild(detailOptions);
            // The changed marker only means something beside settings, so it closes the detail column.
            var legend:TextField = label(tr("menu.changed"), MenuStyle.DETAIL_X, MenuStyle.LIST_BOTTOM - MenuStyle.SMALL_SIZE - 14, detailWidth,
                MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true);
            changedLegend.mouseEnabled = false; changedLegend.mouseChildren = false;
            addChild(changedLegend); changedLegend.addChild(legend);
            alignRight(legend);
            MenuStyle.diamond(changedLegend.graphics, MenuStyle.RIGHT - legend.textWidth - 22, legend.y + legend.height / 2 - 1, MenuStyle.ACCENT);
        }
        CONFIG::testHarness { public function get hintText():String { return visible ? detailHint.text : ""; } }
        private function label(text:String, x:Number, y:Number, width:Number, height:Number,
                               size:Number, color:uint = 0xF1F2EC, isLabel:Boolean = false):TextField
        {
            var field:TextField = MenuStyle.field(text, x, y, width, height, size, color, isLabel);
            addChild(field); return field;
        }
        private function alignRight(field:TextField):void
        {
            var format:TextFormat = field.defaultTextFormat; format.align = "right";
            field.defaultTextFormat = format; field.setTextFormat(format);
        }
        public function show(row:Object, top:Number, editing:Boolean):void
        {
            var value:* = row && row.type == "enum" ? row.value : undefined;
            if (selected == row && selectedValue === value && wasEditing == editing && previousTop == top) return;
            var scroll:int = selected && row && selected.mod == row.mod && selected.key == row.key ? detailHint.scrollV : 1;
            selected = row; selectedValue = value; wasEditing = editing; previousTop = top;
            MenuStyle.setText(detailLabel, row && row.type == "action" ? tr("menu.selectedAction") : tr("menu.selectedSetting"));
            defaultLabel.visible = defaultValue.visible = detailDivider.visible = changedLegend.visible = true;
            // The column flows top-down from the list top: title, hint, then the value facts.
            detailLabel.y = top - 4;
            detailTitle.y = detailLabel.y + MenuStyle.SMALL_SIZE + 12;
            MenuStyle.setText(detailTitle, row ? row.title : tr("menu.nothingSelected"));
            detailTitle.height = detailTitle.textHeight + 8;
            var hint:String = row ? String(row.hint || "") : "";
            if (row && row.type == "action" && row.message) hint += (hint ? "\n\n" : "") + row.message;
            hint = (row && row.requiresRestart ? tr("menu.restart") + (hint ? "\n\n" : "") : "") + hint;
            detailHint.y = detailTitle.y + detailTitle.height + 4;
            MenuStyle.setText(detailHint, hint);
            // Long hints scroll with the wheel rather than pushing the facts off the column.
            detailHint.height = hint ? Math.min(detailHint.textHeight + 10, CONFIG::largeText ? 300 : 260) : 0;
            detailHint.scrollV = scroll;
            var cursor:Number = detailHint.y + detailHint.height + (hint ? 16 : 8);
            detailDivider.y = cursor; cursor += 14;
            // Enum defaults are marked in the option list instead.
            var enumRow:Boolean = Boolean(row && row.type == "enum");
            if (row && row.type == "action" || enumRow) defaultLabel.visible = defaultValue.visible = false;
            if (row && row.type == "action") { detailDivider.visible = false; changedLegend.visible = false; }
            defaultLabel.y = cursor; defaultValue.y = cursor - 1;
            MenuStyle.fit(defaultValue, row && row.type == "hotkey" ? row.defaultName : row ? NumericSetting.text(row, row.defaultValue) : "");
            if (defaultLabel.visible) cursor += MenuStyle.VALUE_SIZE + 16;
            var range:String = row ? NumericSetting.range(row) : "";
            rangeLabel.visible = rangeValue.visible = Boolean(range);
            rangeLabel.y = cursor; rangeValue.y = cursor - 1; MenuStyle.fit(rangeValue, range);
            if (range) cursor += MenuStyle.VALUE_SIZE + 16;
            showOptions(enumRow ? row : null, cursor);
            editorTop = cursor + 6;
            detailDecoration.graphics.clear();
            var decorationBottom:Number = MenuStyle.LIST_BOTTOM - MenuStyle.SMALL_SIZE - 26;
            MenuDecoration.corners(detailDecoration.graphics, MenuStyle.DETAIL_X - 10, detailLabel.y - 2,
                MenuStyle.DETAIL_WIDTH + 20, decorationBottom - detailLabel.y + 2);
            var contentBottom:Number = Math.max(cursor, detailOptions.visible ? detailOptions.y + detailOptions.height : cursor);
            // Text editing occupies the detail column until the editor closes.
            if (!editing) MenuDecoration.orbit(detailDecoration.graphics, MenuStyle.DETAIL_X,
                contentBottom + 24, MenuStyle.DETAIL_WIDTH, decorationBottom - 16);
        }
        // The selected enum's choices, with the current one filled and the default tagged.
        private function showOptions(row:Object, top:Number):void
        {
            var labels:Array = row ? EnumSetting.labels(row) : [];
            while (detailOptions.numChildren) detailOptions.removeChildAt(0);
            detailOptions.graphics.clear();
            detailOptions.visible = Boolean(row);
            if (!row) return;
            detailOptions.y = top;
            var columnWidth:Number = MenuStyle.DETAIL_WIDTH;
            detailOptions.addChild(MenuStyle.field(tr("menu.options"), 0, 0, columnWidth, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true));
            var pitch:Number = MenuStyle.VALUE_SIZE + 14; var first:Number = MenuStyle.SMALL_SIZE + 16;
            // Stop above the changed-from-default legend that closes the column.
            var room:int = Math.max(1, Math.floor((MenuStyle.LIST_BOTTOM - MenuStyle.SMALL_SIZE - 30 - top - first) / pitch));
            var shown:int = labels.length > room ? room - 1 : labels.length;
            for (var i:int = 0; i < shown; ++i) {
                var line:Number = first + i * pitch;
                var selected:Boolean = row.options[i].value === row.value;
                detailOptions.graphics.lineStyle(2, selected ? MenuStyle.WHITE : MenuStyle.LINE);
                if (selected) detailOptions.graphics.beginFill(MenuStyle.WHITE);
                detailOptions.graphics.drawCircle(7, line + pitch / 2 - 3, 5);
                if (selected) detailOptions.graphics.endFill();
                var option:TextField = MenuStyle.field("", 26, line, columnWidth - 170, pitch, MenuStyle.VALUE_SIZE, selected ? MenuStyle.WHITE : MenuStyle.MUTED, true);
                detailOptions.addChild(option); MenuStyle.fit(option, String(labels[i]));
                if (row.options[i].value === row.defaultValue) {
                    var tag:TextField = MenuStyle.field(tr("menu.default"), columnWidth - 160, line + 2, 160, pitch, MenuStyle.SMALL_SIZE - 1, MenuStyle.MUTED, true);
                    alignRight(tag); detailOptions.addChild(tag);
                }
            }
            if (shown < labels.length)
                detailOptions.addChild(MenuStyle.field(tr("menu.moreOptions", {count:labels.length - shown}), 26, first + shown * pitch,
                    columnWidth - 26, pitch, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true));
        }
        private function scrollDescription(event:MouseEvent):void
        {
            detailHint.scrollV -= event.delta; event.stopPropagation();
        }
    }
}
