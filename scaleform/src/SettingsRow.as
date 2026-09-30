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

        public function SettingsRow()
        {
            mouseEnabled = false; mouseChildren = false;
            title = MenuStyle.field("", 16, 0, 650, MenuStyle.BODY_SIZE + 14, MenuStyle.BODY_SIZE);
            value = MenuStyle.field("", 0, 0, 150, MenuStyle.BODY_SIZE + 14, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true);
            var centered:TextFormat = value.defaultTextFormat; centered.align = "center";
            value.defaultTextFormat = centered; addChild(title); addChild(value);
            source = MenuStyle.field("", 16, 0, 510, MenuStyle.SMALL_SIZE + 10, MenuStyle.SMALL_SIZE, MenuStyle.MUTED, true); addChild(source);
            heading = MenuStyle.field("", 24, 0, 600, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED, true); addChild(heading);
        }
        // Columns are anchored to the list's right edge so they follow LIST_WIDTH.
        public function update(row:Object, selected:Boolean, modList:Boolean, rowHeight:Number, hovered:Boolean = false):void
        {
            var rowWidth:Number = MenuStyle.LIST_WIDTH;
            var section:Boolean = row.type == "section";
            var changed:Boolean = !modList && !section && row.type != "hotkey" && row.type != "action" && row.value != row.defaultValue;
            var slider:Boolean = !modList && NumericSetting.isSlider(row);
            var displayValue:String = section ? "" : modList ? String(row.summary) : row.type == "action" ? row.actionState : row.type == "enum" ? EnumSetting.text(row, row.value) : NumericSetting.text(row, row.value);
            graphics.clear();
            heading.visible = section;
            title.visible = value.visible = !section;
            if (section) {
                // Headers split a folded tab; the menu keeps selection off them.
                source.visible = false;
                heading.width = rowWidth - heading.x; MenuStyle.fit(heading, String(row.title).toUpperCase());
                heading.width = Math.min(rowWidth - heading.x, heading.textWidth + 8);
                heading.y = rowHeight - heading.height - 2;
                var rule:Number = heading.y + heading.height / 2 + 1;
                MenuStyle.diamond(graphics, 8, rule - 2, MenuStyle.ACCENT);
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
                source.width = MenuStyle.CONTROL_X - 70;
                MenuStyle.fit(source,row.source);
            }
            value.visible = !row.capturing && row.type != "hotkey";
            var color:uint = selected ? MenuStyle.INK : MenuStyle.WHITE;
            graphics.beginFill(selected ? MenuStyle.WHITE : hovered ? MenuStyle.HOVER : MenuStyle.ROW);
            graphics.drawRect(0, 0, rowWidth, rowHeight); graphics.endFill();
            title.textColor = color; value.textColor = modList ? (selected ? 0x3D4F58 : MenuStyle.MUTED) : color;
            title.width = modList ? rowWidth - 576 : MenuStyle.CONTROL_X - 70;
            // Mod rows summarize what each mod adds, right-aligned before the chevron.
            var format:TextFormat = value.defaultTextFormat; format.align = modList ? "right" : "center";
            format.size = modList ? MenuStyle.SMALL_SIZE + 1 : MenuStyle.VALUE_SIZE; value.defaultTextFormat = format;
            value.x = modList ? rowWidth - 546 : MenuStyle.CONTROL_X +
                (slider ? MenuStyle.CONTROL_WIDTH - MenuStyle.SLIDER_VALUE_WIDTH : MenuStyle.VALUE_INSET);
            value.y = (rowHeight - Number(format.size)) / 2 - 3;
            value.width = modList ? 500 : slider ? MenuStyle.SLIDER_VALUE_WIDTH : MenuStyle.VALUE_WIDTH;
            MenuStyle.fit(title, String(row.title));
            MenuStyle.fit(value, displayValue);
            var center:Number = rowHeight / 2;
            graphics.lineStyle(2, selected ? 0x52636C : 0xA0B0B8);
            if (!modList && row.type == "bool") {
                var leftArrow:Number = MenuStyle.CONTROL_X + 22;
                var rightArrow:Number = MenuStyle.CONTROL_X + MenuStyle.CONTROL_WIDTH - 22;
                graphics.moveTo(leftArrow + 2, center - 5); graphics.lineTo(leftArrow - 2, center); graphics.lineTo(leftArrow + 2, center + 5);
                graphics.moveTo(rightArrow - 2, center - 5); graphics.lineTo(rightArrow + 2, center); graphics.lineTo(rightArrow - 2, center + 5);
            }
            if (modList) { graphics.moveTo(rowWidth - 27, center - 5); graphics.lineTo(rowWidth - 23, center); graphics.lineTo(rowWidth - 27, center + 5); }
            graphics.lineStyle();
            if (changed) MenuStyle.diamond(graphics, Math.min(title.x + title.textWidth + 18, MenuStyle.CONTROL_X - 32), center, MenuStyle.ACCENT);
        }
    }
}
