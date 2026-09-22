package
{
    import flash.display.Sprite;
    import flash.text.TextField;

    // Read-only presentation; vanilla owns selection, hit areas and scrolling.
    public final class IssueRow extends Sprite
    {
        private var title:TextField;
        private var mod:TextField;
        private var severity:TextField;
        private var previous:Object;
        private var wasSelected:Boolean;

        public function IssueRow()
        {
            mouseEnabled = false; mouseChildren = false;
            title = MenuStyle.field("", 20, 8, 690, 44, MenuStyle.BODY_SIZE);
            mod = MenuStyle.field("", 20, CONFIG::largeText ? 53 : 43, 690, 36, CONFIG::largeText ? 25 : 22, MenuStyle.MUTED);
            severity = MenuStyle.field("", 806, (MenuStyle.ROW_HEIGHT - 32) / 2, 190, 38, CONFIG::largeText ? 28 : 25, MenuStyle.WHITE, true);
            addChild(title); addChild(mod); addChild(severity);
        }

        public function update(row:Object, selected:Boolean, modList:Boolean):void
        {
            if (previous == row && wasSelected == selected) return;
            previous = row; wasSelected = selected;
            var color:uint = selected ? MenuStyle.INK : MenuStyle.WHITE;
            title.textColor = color;
            mod.textColor = selected ? MenuStyle.LINE : MenuStyle.MUTED;
            severity.textColor = selected ? MenuStyle.INK : MenuStyle.ACCENT;
            MenuStyle.fit(title, row.title); MenuStyle.fit(mod, row.modTitle);
            MenuStyle.fit(severity, row.severityLabel || tr(row.severity == "ERROR" ? "issues.error" : "issues.warning"));
            graphics.clear(); graphics.beginFill(selected ? MenuStyle.WHITE : MenuStyle.ROW);
            graphics.drawRect(0, 0, MenuStyle.LIST_WIDTH, MenuStyle.ROW_HEIGHT); graphics.endFill();
            var x:Number = 768; var y:Number = MenuStyle.ROW_HEIGHT / 2;
            graphics.lineStyle(2, severity.textColor);
            if (row.severity == "ERROR") graphics.drawCircle(x, y, 14);
            else {
                graphics.moveTo(x, y - 15); graphics.lineTo(x + 16, y + 13);
                graphics.lineTo(x - 16, y + 13); graphics.lineTo(x, y - 15);
            }
            graphics.moveTo(x, y - 6); graphics.lineTo(x, y + 3);
            graphics.moveTo(x, y + 7); graphics.lineTo(x, y + 8);
            graphics.lineStyle();
        }
    }
}
