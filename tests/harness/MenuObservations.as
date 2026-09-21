// Included inside OSFSettingsMenu only for CONFIG::testHarness builds.

private var testCaptureState:String = "idle";
private var testMouseDown:Object = {sequence:0};
private var testMouseClick:Object = {sequence:0};
private var testTranslations:Object;
private var testTranslationRevision:uint = uint.MAX_VALUE;

private function testTranslationState():Object
{
    if (testTranslations && testTranslationRevision == nativeHotkeys.revision) return testTranslations;
    testTranslationRevision = nativeHotkeys.revision;
    var keys:Object = {heading:"$OSFModBindings", action:"$OSFModBindings_osfsettings/openMenu",
        originalContext:"$MainGameplay_osfsettings/openMenu", kbm:"$OSFModBindings_osfsettings/openMenu_KBM",
        gamepad:"$OSFModBindings_osfsettings/openMenu_GP", unicode:"$OSFTranslationProbe_Unicode",
        unknown:"$OSFTranslationProbe_Unknown", vanilla:"$MainGameplay_Forward", requiredHeading:"$$OSFModBindings *"};
    testTranslations = {};
    var field:TextField = new TextField();
    var global:Object = definition("Shared.GlobalFunc");
    for (var name:String in keys) {
        global.SetText(field, keys[name]);
        // Compare supplementary text as exact AS3 UTF-16 units. GFx's narrow
        // string export is not necessarily valid Unicode-scalar UTF-8 for pairs.
        if (name == "unicode") {
            var units:Array = [];
            for (var i:int = 0; i < field.text.length; ++i) units.push(field.text.charCodeAt(i));
            testTranslations[name] = units;
        } else testTranslations[name] = field.text;
    }
    var nativeRows:Array = [];
    for each (var entry:Object in nativeHotkeys.bindings) {
        if (nativeRows.length >= 16) break;
        nativeRows.push({divider:Boolean(entry.bIsDivider), context:entry.uContextID,
            presentationContext:entry.sContextName, action:entry.sInputName, title:nativeHotkeys.title(entry)});
    }
    testTranslations.nativeRows = nativeRows;
    return testTranslations;
}

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
    return {kind:bindingsPage() ? "binding" : issuesPage() ? "issue" : modID ? "setting" : "mod", mod:row.mod, group:String(row.group || ""),
        issueId:String(row.id || ""), severity:String(row.severity || ""), impact:String(row.impact || ""), nextSteps:String(row.nextSteps || ""),
        key:String(row.key || ""), type:String(row.type || ""), value:row.value,
        action:row.action, source:row.source, potential:Boolean(row.potential), records:row.records,
        alternate:row.alternate, required:Boolean(row.binding && row.binding.bRequired), title:row.title, editable:Boolean(row.editable), minimum:row.minimum, maximum:row.maximum};
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
    var requiredActions:Array = [];
    for each (var bindingRow:Object in keybindings.rows) if (bindingRow.binding.bRequired && bindingRow.editable)
        requiredActions.push({action:bindingRow.action,title:bindingRow.title});
    var visible:Rectangle = Object(extensions).visibleRect as Rectangle;
    BGSCodeObj.testSnapshot(frame, {initialized:initialized, closing:closing,
        largeText:CONFIG::largeText, emptyText:empty ? empty.text : "", detailHint:detailHint ? detailHint.text : "",
        stringEditor:stringEditor.testState(), issueDetails:issueDetails.testState(),
        translations:testTranslationState(),
        conflictText:nativeHotkeys.popup.active ? String(nativeHotkeys.popup.ControlInfo_mc.Label_mc.Text_tf.text) : "",
        refreshing:refreshing || requestedRefresh, startupPhase:startupPhase,
        mod:modID, group:groupID, rootPage:rootPage, issueCount:issues.length,
        bindings:{state:keybindings.state, count:keybindings.rows.length, requiredActions:requiredActions, selectedKey:keybindings.selectedKey,
            source:keybindings.source, query:keybindings.search.text, searching:searching(), slot:nativeHotkeys.selectedSlot,
            searchRect:testRect(keybindings.search.getBounds(menuStage)),
            sourceRect:testRect(keybindings.getChildByName("bindingSource").getBounds(menuStage)),
            clearRect:testRect(keybindings.getChildByName("clearBindingFilters").getBounds(menuStage)),
            primaryRect:testRect(nativeHotkeys.slotRect(0)), alternateRect:testRect(nativeHotkeys.slotRect(1))},
        selectedIndex:options ? options.selectedIndex : -1,
        scrollPosition:options ? options.scrollPosition : 0, selection:testRow(current()), rows:visibleRows,
        stage:{width:1920, height:1080, visibleRect:visible ? testRect(visible) : null},
        mouse:{x:menuStage.mouseX, y:menuStage.mouseY}, mouseDown:testMouseDown, mouseClick:testMouseClick,
        capture:{active:Boolean(captureRow) || nativeHotkeys.busy, ready:captureReady, saving:nativeHotkeys.saving,
            state:nativeHotkeys.busy ? nativeHotkeys.popup.active ? "conflict" : "listening" : testCaptureState,
            mod:captureRow ? captureRow.mod : nativeHotkeys.busy && current() ? current().mod : "",
            key:captureRow ? captureRow.key : nativeHotkeys.busy && current() ? current().key : ""},
        status:status ? status.text : ""});
}
