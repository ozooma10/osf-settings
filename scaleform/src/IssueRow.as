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
        private var wasHovered:Boolean;

        public function IssueRow()
        {
            mouseEnabled = false; mouseChildren = false;
            var rowWidth:Number = IssueStyle.LIST_WIDTH;
            title = MenuStyle.field("", 82, 18, rowWidth - 246, MenuStyle.BODY_SIZE + 14, MenuStyle.BODY_SIZE);
            mod = MenuStyle.field("", 82, 0, rowWidth - 246, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED);
            severity = MenuStyle.field("", rowWidth - 140, 0, 126, MenuStyle.VALUE_SIZE + 12, MenuStyle.VALUE_SIZE, MenuStyle.WHITE, true);
            addChild(title); addChild(mod); addChild(severity);
        }

        public function update(row:Object, selected:Boolean, modList:Boolean, rowHeight:Number, hovered:Boolean = false):void
        {
            if (previous == row && wasSelected == selected && wasHovered == hovered) return;
            previous = row; wasSelected = selected; wasHovered = hovered;
            var boxHeight:Number = rowHeight - 10;
            var textHeight:Number = MenuStyle.BODY_SIZE + MenuStyle.SMALL_SIZE + 18;
            title.y = (boxHeight - textHeight) / 2;
            mod.y = title.y + MenuStyle.BODY_SIZE + 12;
            severity.y = (boxHeight - severity.height) / 2 + 2;
            title.textColor = MenuStyle.WHITE;
            mod.textColor = MenuStyle.MUTED;
            severity.textColor = IssueStyle.color(row.severity);
            MenuStyle.fit(title, row.title); MenuStyle.fit(mod, row.modTitle);
            MenuStyle.fit(severity, row.severityLabel || tr(row.severity == "ERROR" ? "issues.error" : "issues.warning"));
            graphics.clear(); graphics.beginFill(selected ? MenuStyle.ROW : hovered ? 0x182A33 : 0x0E1B23);
            graphics.drawRect(0, 0, IssueStyle.LIST_WIDTH, boxHeight); graphics.endFill();
            if (selected) {
                graphics.beginFill(severity.textColor); graphics.drawRect(1, 1, 5, boxHeight - 2); graphics.endFill();
                MenuDecoration.corners(graphics, 1, 1, IssueStyle.LIST_WIDTH - 2, boxHeight - 2, MenuStyle.WHITE);
            }
            IssueStyle.icon(graphics, 42, boxHeight / 2, row.severity == "ERROR", 22);
        }
    }
}
