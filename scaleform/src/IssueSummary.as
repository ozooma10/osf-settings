package
{
    import flash.display.Sprite;
    import flash.text.TextField;

    public final class IssueSummary extends Sprite
    {
        private var errors:TextField;
        private var warnings:TextField;

        public function IssueSummary()
        {
            x = MenuStyle.LEFT; y = MenuStyle.SECTION_TOP;
            mouseEnabled = mouseChildren = false;
            errors = MenuStyle.field("", 42, 0, 300, 40, MenuStyle.SECTION_SIZE, IssueStyle.ERROR, true);
            warnings = MenuStyle.field("", 0, 0, 300, 40, MenuStyle.SECTION_SIZE, IssueStyle.WARNING, true);
            addChild(errors); addChild(warnings);
        }

        public function update(rows:Array):void
        {
            var errorCount:int = 0;
            for each (var row:Object in rows) if (row.severity == "ERROR") ++errorCount;
            MenuStyle.setText(errors, tr("issues.error") + "  " + errorCount);
            MenuStyle.setText(warnings, tr("issues.warning") + "  " + (rows.length - errorCount));
            var divider:Number = errors.x + errors.textWidth + 30;
            warnings.x = divider + 64;
            graphics.clear();
            var center:Number = MenuStyle.SECTION_SIZE / 2 + 4;
            IssueStyle.icon(graphics, 16, center, true, 14);
            IssueStyle.icon(graphics, divider + 38, center, false, 14);
            graphics.lineStyle(1, MenuStyle.LINE);
            graphics.moveTo(divider, 0); graphics.lineTo(divider, MenuStyle.SECTION_SIZE + 8);
            graphics.lineStyle();
        }
    }
}
