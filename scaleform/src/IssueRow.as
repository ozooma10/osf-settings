package
{
    import flash.display.Sprite;
    import flash.text.TextField;

    // Read-only presentation; vanilla owns selection, hit areas and scrolling.
    public final class IssueRow extends Sprite
    {
        private var title:TextField;
        private var mod:TextField;

        public function IssueRow()
        {
            mouseEnabled = false; mouseChildren = false;
            // The icon and selection stripe carry severity, so text uses the full row.
            var textWidth:Number = IssueStyle.LIST_WIDTH - 82 - 28;
            title = MenuStyle.field("", 82, 18, textWidth, MenuStyle.BODY_SIZE + 14, MenuStyle.BODY_SIZE);
            mod = MenuStyle.field("", 82, 0, textWidth, MenuStyle.SMALL_SIZE + 12, MenuStyle.SMALL_SIZE + 1, MenuStyle.MUTED);
            addChild(title); addChild(mod);
        }

        public function update(row:Object, selected:Boolean, modList:Boolean, rowHeight:Number, hovered:Boolean = false):void
        {
            var boxHeight:Number = rowHeight - 10;
            var textHeight:Number = MenuStyle.BODY_SIZE + MenuStyle.SMALL_SIZE + 18;
            title.y = (boxHeight - textHeight) / 2;
            mod.y = title.y + MenuStyle.BODY_SIZE + 12;
            title.textColor = MenuStyle.WHITE;
            mod.textColor = MenuStyle.MUTED;
            MenuStyle.fit(title, row.title);
            MenuStyle.fit(mod, IssueStyle.affected(row));
            graphics.clear(); graphics.beginFill(selected ? MenuStyle.ROW : hovered ? 0x182A33 : 0x0E1B23);
            graphics.drawRect(0, 0, IssueStyle.LIST_WIDTH, boxHeight); graphics.endFill();
            if (selected) {
                graphics.beginFill(IssueStyle.color(row.severity)); graphics.drawRect(1, 1, 5, boxHeight - 2); graphics.endFill();
                MenuDecoration.corners(graphics, 1, 1, IssueStyle.LIST_WIDTH - 2, boxHeight - 2, MenuStyle.WHITE);
            }
            IssueStyle.icon(graphics, 42, boxHeight / 2, row.severity == "ERROR", 22);
        }
    }
}
