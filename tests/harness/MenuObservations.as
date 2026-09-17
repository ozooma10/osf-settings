// Included inside OSFSettingsMenu only for CONFIG::testHarness builds.

private var testCaptureState:String = "idle";
private var testMouseDown:Object = {sequence:0};
private var testMouseClick:Object = {sequence:0};

private function advanceTestObservations():void
{
    // Observations must never prevent the normal movie from advancing.
    try { reportTestState(); }
    catch (error:Error) {
        try { BGSCodeObj.testSnapshot(frame, {initialized:initialized, diagnosticError:error.toString()}); }
        catch (ignored:Error) {}
    }
}
private function testRect(rect:Rectangle):Object
{
    return {x:rect.x, y:rect.y, width:rect.width, height:rect.height};
}
private function testMouseEvent(event:MouseEvent, previous:Object):Object
{
    var target:DisplayObject = event.target as DisplayObject;
    var path:String = "";
    for (var i:int = 0; target && i < 8; ++i) {
        path = (path ? path + "/" : "") + target.name;
        target = target.parent;
    }
    return {sequence:Number(previous.sequence) + 1, frame:frame, x:event.stageX, y:event.stageY,
        target:path, buttonDown:event.buttonDown};
}
private function testClick(event:MouseEvent):void
{
    testMouseClick = testMouseEvent(event, testMouseClick);
}
private function testRow(row:Object):Object
{
    if (!row) return null;
    return {kind:modID ? "setting" : "mod", mod:row.mod, group:String(row.group || ""),
        key:String(row.key || ""), type:String(row.type || ""), value:row.value,
        title:row.title, editable:Boolean(row.editable), minimum:row.minimum, maximum:row.maximum};
}
private function reportTestState():void
{
    if (!bridgeReady || !("testSnapshot" in BGSCodeObj)) return;
    // Copy only on the movie lane. Geometry is sampled at 10 Hz, with a
    // heartbeat every frame so stalled rendering remains distinguishable.
    if (frame % 6 != 1) { BGSCodeObj.testSnapshot(frame, null); return; }
    var visibleRows:Array = [];
    if (options && menuStage) {
        for (var i:int = 0; i < options.totalEntryClips; ++i) {
            var clip:MovieClip = options.GetClipByIndex(i) as MovieClip;
            if (!clip || !clip.visible || Object(clip).itemIndex < 0) continue;
            var item:Object = options.GetDataForEntry(Object(clip).itemIndex);
            if (!item) continue;
            var row:Object = testRow(item.row); row.index = Object(clip).itemIndex;
            // Border is the actual vanilla row hit area, without off-row text bounds.
            row.rect = testRect(DisplayObject(Object(clip).Border_mc).getBounds(menuStage));
            var control:DisplayObject = NumericSetting.isSlider(item.row) ? Object(clip).Slider_mc as DisplayObject : null;
            row.controlRect = control ? testRect(control.getBounds(menuStage)) : row.rect;
            visibleRows.push(row);
        }
    }
    var extensions:Class = definition("scaleform.gfx.Extensions");
    var visible:Rectangle = Object(extensions).visibleRect as Rectangle;
    BGSCodeObj.testSnapshot(frame, {initialized:initialized, closing:closing,
        refreshing:refreshing || requestedRefresh, startupPhase:startupPhase,
        mod:modID, group:groupID, selectedIndex:options ? options.selectedIndex : -1,
        scrollPosition:options ? options.scrollPosition : 0, selection:testRow(current()), rows:visibleRows,
        stage:{width:1920, height:1080, visibleRect:visible ? testRect(visible) : null},
        mouse:{x:menuStage.mouseX, y:menuStage.mouseY}, mouseDown:testMouseDown, mouseClick:testMouseClick,
        capture:{active:Boolean(captureRow), ready:captureReady, state:testCaptureState,
            mod:captureRow ? captureRow.mod : "", key:captureRow ? captureRow.key : ""},
        status:status ? status.text : ""});
}
