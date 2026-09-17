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
        private var previous:String = "";

        public function SettingsRow()
        {
            mouseEnabled = false; mouseChildren = false;
            var top:Number = (MenuStyle.ROW_HEIGHT - MenuStyle.BODY_SIZE) / 2 - 3;
            title = MenuStyle.field("", 20, top, 650, 46, MenuStyle.BODY_SIZE);
            value = MenuStyle.field("", 776, top, 150, 46, CONFIG::largeText ? 28 : 25, MenuStyle.WHITE, true);
            var centered:TextFormat = value.defaultTextFormat; centered.align = "center";
            value.defaultTextFormat = centered; addChild(title); addChild(value);
        }
        public function update(row:Object, selected:Boolean, modList:Boolean):void
        {
            var changed:Boolean = !modList && row.value != row.defaultValue;
            var slider:Boolean = !modList && NumericSetting.isSlider(row);
            var choice:Boolean = !modList && (row.type == "enum" || row.type == "key");
            var displayValue:String = modList ? String(row.count) : row.type == "enum" ? EnumSetting.text(row, row.value) : NumericSetting.text(row, row.value);
            var signature:String = [row.title, row.value, row.type, row.editable, row.decimals, row.count, displayValue, changed, selected, modList, row.capturing].join("|");
            if (signature == previous) return;
            previous = signature;
            value.visible = !row.capturing;
            var color:uint = selected ? MenuStyle.INK : MenuStyle.WHITE;
            graphics.clear(); graphics.beginFill(selected ? MenuStyle.WHITE : MenuStyle.ROW);
            graphics.drawRect(0, 0, MenuStyle.LIST_WIDTH, MenuStyle.ROW_HEIGHT); graphics.endFill();
            title.textColor = color; value.textColor = color;
            title.width = slider || choice ? 470 : 650;
            value.x = choice ? 580 : slider ? 888 : NumericSetting.isNumeric(row) ? 694 : 776;
            value.width = choice ? 370 : slider ? 100 : NumericSetting.isNumeric(row) ? 300 : 150;
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
