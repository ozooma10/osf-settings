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
        private var previous:String = "";

        public function SettingsRow()
        {
            mouseEnabled = false; mouseChildren = false;
            var top:Number = (MenuStyle.ROW_HEIGHT - MenuStyle.BODY_SIZE) / 2 - 3;
            title = MenuStyle.field("", 20, top, 650, 46, MenuStyle.BODY_SIZE);
            value = MenuStyle.field("", 776, top, 150, 46, CONFIG::largeText ? 28 : 25, MenuStyle.WHITE, true);
            var centered:TextFormat = value.defaultTextFormat; centered.align = "center";
            value.defaultTextFormat = centered; addChild(title); addChild(value);
            source = MenuStyle.field("",20,MenuStyle.ROW_HEIGHT - 34,510,30,CONFIG::largeText ? 24 : 20,MenuStyle.MUTED,true); addChild(source);
        }
        public function update(row:Object, selected:Boolean, modList:Boolean):void
        {
            var changed:Boolean = !modList && row.type != "hotkey" && row.type != "action" && row.value != row.defaultValue;
            var slider:Boolean = !modList && NumericSetting.isSlider(row);
            var choice:Boolean = !modList && (row.type == "enum" || row.type == "key" || row.type == "hotkey" || row.type == "string" || row.type == "action");
            var displayValue:String = modList ? String(row.summary) : row.type == "action" ? row.actionState : row.type == "enum" ? EnumSetting.text(row, row.value) : NumericSetting.text(row, row.value);
            var signature:String = [row.title, row.value, row.type, row.editable, row.decimals, row.count, displayValue, changed, selected, modList, row.capturing, row.keybindings, row.source, row.mod].join("|");
            if (signature == previous) return;
            previous = signature;
            source.visible = Boolean(row.keybindings);
            title.y = row.keybindings ? 3 : (MenuStyle.ROW_HEIGHT - MenuStyle.BODY_SIZE) / 2 - 3;
            if (row.keybindings) {
                // Orange matches the keyboard's mod-owned key marker.
                source.textColor = selected ? MenuStyle.INK : row.mod ? MenuStyle.ACCENT : MenuStyle.MUTED;
                MenuStyle.fit(source,row.source);
            }
            value.visible = !row.capturing && row.type != "hotkey";
            var color:uint = selected ? MenuStyle.INK : MenuStyle.WHITE;
            graphics.clear(); graphics.beginFill(selected ? MenuStyle.WHITE : MenuStyle.ROW);
            graphics.drawRect(0, 0, MenuStyle.LIST_WIDTH, MenuStyle.ROW_HEIGHT); graphics.endFill();
            title.textColor = color; value.textColor = modList ? (selected ? 0x3D4F58 : MenuStyle.MUTED) : color;
            title.width = modList ? 440 : slider || choice ? 470 : 650;
            // Mod rows summarize what each mod adds, right-aligned before the chevron.
            var format:TextFormat = value.defaultTextFormat; format.align = modList ? "right" : "center";
            format.size = modList ? (CONFIG::largeText ? 24 : 21) : (CONFIG::largeText ? 28 : 25); value.defaultTextFormat = format;
            value.x = modList ? 470 : choice ? 580 : slider ? 888 : NumericSetting.isNumeric(row) ? 694 : 776;
            value.y = (MenuStyle.ROW_HEIGHT - MenuStyle.BODY_SIZE) / 2 - (modList ? 0 : 3);
            value.width = modList ? 500 : choice ? 370 : slider ? 100 : NumericSetting.isNumeric(row) ? 300 : 150;
            MenuStyle.fit(title, String(row.title));
            MenuStyle.fit(value, displayValue);
            var center:Number = MenuStyle.ROW_HEIGHT / 2;
            graphics.lineStyle(2, selected ? 0x52636C : 0xA0B0B8);
            if (!modList && row.type == "bool") {
                graphics.moveTo(714, center - 5); graphics.lineTo(710, center); graphics.lineTo(714, center + 5);
            }
            if (modList || row.type == "bool") { graphics.moveTo(989, center - 5); graphics.lineTo(993, center); graphics.lineTo(989, center + 5); }
            graphics.lineStyle();
            if (changed) MenuStyle.diamond(graphics, Math.min(title.x + title.textWidth + 18, slider || choice ? 508 : 683), center, MenuStyle.ACCENT);
        }
    }
}
