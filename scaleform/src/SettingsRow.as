package
{
    import flash.display.Sprite;
    import flash.text.TextField;
    import flash.text.TextFormat;

    // Selection, scrolling and activation belong to the vanilla scrolling list.
    // This clip only presents its entry in the menu's two-column layout.
    public final class SettingsRow extends Sprite
    {
        private var title:TextField;
        private var value:TextField;
        private var source:TextField;
        private var heading:TextField;
        private var previous:String = "";

        public function SettingsRow()
        {
            mouseEnabled = false; mouseChildren = false;
            title = MenuStyle.field("", 16, 0, 650, MenuStyle.BODY_SIZE + 14, MenuStyle.BODY_SIZE);
            value = MenuStyle.field("", 0, 0, 150, MenuStyle.BODY_SIZE + 14, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true);
            var centered:TextFormat = value.defaultTextFormat; centered.align = "center";
            value.defaultTextFormat = centered; addChild(title); addChild(value);
            source = MenuStyle.field("", 16, 0, 510, MenuStyle.SMALL_SIZE + 10, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true); addChild(source);
            heading = MenuStyle.field("", 2, 0, 600, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true); addChild(heading);
        }
        // Columns are anchored to the list's right edge so they follow LIST_WIDTH.
        public function update(row:Object, selected:Boolean, modList:Boolean, rowHeight:Number):void
        {
            var rowWidth:Number = MenuStyle.LIST_WIDTH;
            var section:Boolean = row.type == "section";
            var changed:Boolean = !modList && !section && row.type != "hotkey" && row.type != "action" && row.value != row.defaultValue;
            var slider:Boolean = !modList && NumericSetting.isSlider(row);
            var choice:Boolean = !modList && (row.type == "enum" || row.type == "key" || row.type == "hotkey" || row.type == "string" || row.type == "action");
            var displayValue:String = section ? "" : modList ? String(row.summary) : row.type == "action" ? row.actionState : row.type == "enum" ? EnumSetting.text(row, row.value) : NumericSetting.text(row, row.value);
            var signature:String = [row.title, row.value, row.type, row.editable, row.decimals, row.count, displayValue, changed, selected, modList, row.capturing, row.keybindings, row.source, row.mod, rowHeight].join("|");
            if (signature == previous) return;
            previous = signature;
            graphics.clear();
            heading.visible = section;
            title.visible = value.visible = !section;
            if (section) {
                // Headers split a folded tab; the menu keeps selection off them.
                source.visible = false;
                heading.width = rowWidth; MenuStyle.fit(heading, String(row.title).toUpperCase());
                heading.width = Math.min(rowWidth, heading.textWidth + 8);
                heading.y = rowHeight - heading.height - 2;
                var rule:Number = heading.y + heading.height / 2 + 1;
                graphics.lineStyle(1, 0x33454E); graphics.moveTo(heading.x + heading.width + 12, rule); graphics.lineTo(rowWidth, rule);
                graphics.lineStyle(); return;
            }
            var lineTop:Number = (rowHeight - MenuStyle.BODY_SIZE) / 2 - 3;
            source.visible = Boolean(row.keybindings);
            title.y = row.keybindings ? 4 : lineTop;
            if (row.keybindings) {
                // Orange matches the keyboard's mod-owned key marker.
                source.y = rowHeight - MenuStyle.SMALL_SIZE - 13;
                source.textColor = selected ? MenuStyle.INK : row.mod ? MenuStyle.ACCENT : MenuStyle.MUTED;
                MenuStyle.fit(source,row.source);
            }
            value.visible = !row.capturing && row.type != "hotkey";
            var color:uint = selected ? MenuStyle.INK : MenuStyle.WHITE;
            graphics.beginFill(selected ? MenuStyle.WHITE : MenuStyle.ROW);
            graphics.drawRect(0, 0, rowWidth, rowHeight); graphics.endFill();
            title.textColor = color; value.textColor = modList ? (selected ? 0x3D4F58 : MenuStyle.MUTED) : color;
            title.width = modList ? rowWidth - 576 : slider || choice ? rowWidth - 546 : rowWidth - 366;
            // Mod rows summarize what each mod adds, right-aligned before the chevron.
            var format:TextFormat = value.defaultTextFormat; format.align = modList ? "right" : "center";
            format.size = modList ? MenuStyle.SMALL_SIZE + 1 : MenuStyle.VALUE_SIZE; value.defaultTextFormat = format;
            value.x = rowWidth - (modList ? 546 : choice ? 436 : slider ? 128 : NumericSetting.isNumeric(row) ? 322 : 240);
            value.y = lineTop + (modList ? 2 : 1);
            value.width = modList ? 500 : choice ? 370 : slider ? 100 : NumericSetting.isNumeric(row) ? 300 : 150;
            MenuStyle.fit(title, String(row.title));
            MenuStyle.fit(value, displayValue);
            var center:Number = rowHeight / 2;
            graphics.lineStyle(2, selected ? 0x52636C : 0xA0B0B8);
            if (!modList && row.type == "bool") {
                graphics.moveTo(rowWidth - 302, center - 5); graphics.lineTo(rowWidth - 306, center); graphics.lineTo(rowWidth - 302, center + 5);
            }
            if (modList || row.type == "bool") { graphics.moveTo(rowWidth - 27, center - 5); graphics.lineTo(rowWidth - 23, center); graphics.lineTo(rowWidth - 27, center + 5); }
            graphics.lineStyle();
            if (changed) MenuStyle.diamond(graphics, Math.min(title.x + title.textWidth + 18, rowWidth - (slider || choice ? 508 : 333)), center, MenuStyle.ACCENT);
        }
    }
}
