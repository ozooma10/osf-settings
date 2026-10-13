package
{
    import flash.display.MovieClip;
    import flash.events.Event;

    // Exercise the production adapter with an isolated engine feed. Nothing is
    // published to the preview menu's real PEOData subscription or native bridge.
    public final class PreviewGameplayChecks
    {
        private static function option(id:uint, title:String, selected:uint = 0):Object
        {
            return {uID:id, uType:1, bIsPEO:true, bEnabled:true, sText:title,
                sDescription:"First line<br>Second line", stepperData:{uIndex:selected, aStepperOptions:["Easy", "Hard"]},
                peoData:{aRewards:[0, 10], aDescriptions:[]}};
        }

        public static function run(menu:MovieClip, require:Function):void
        {
            var callback:Function;
            var subscriptions:int = 0, removals:int = 0, notifications:int = 0;
            var lastMessage:String = "", requests:Array = [], commits:Array = [];
            var allowCommit:Boolean = true;
            var manager:Object = {
                Subscribe:function(name:String, listener:Function):void {
                    require(name == "PEOData", "gameplay subscribes to the engine's PEO feed");
                    callback = listener; subscriptions++;
                },
                Unsubscribe:function(name:String, listener:Function):void {
                    require(name == "PEOData" && listener == callback, "gameplay disposes its exact subscription");
                    removals++;
                },
                dispatchEvent:function(event:Event):void { requests.push(event.type); },
                dispatchCustomEvent:function(name:String, data:Object):void { requests.push(name + ":" + data.categoryID); }
            };
            var bridge:Object = {
                getGameplaySources:function():Array {
                    return [
                        {file:"Starfield.esm", mask:0xFF000000, prefix:0, official:true, editable:true},
                        {file:"Official.esm", mask:0xFF000000, prefix:0x01000000, official:true, editable:true},
                        {file:"Example.esm", mask:0xFF000000, prefix:0x02000000, official:false, editable:true},
                        {file:"ReadOnly.esm", mask:0xFF000000, prefix:0x03000000, official:false, editable:false}
                    ];
                },
                commitGameplayChange:function(id:uint, type:uint, selected:uint):Boolean {
                    commits.push({id:id, type:type, selected:selected}); return allowCommit;
                }
            };
            var adapterClass:Class = menu.loaderInfo.applicationDomain.getDefinition("NativeGameplayOptions") as Class;
            var adapter:Object = new adapterClass(function(name:String):Object {
                return name == "Shared.AS3.Data.BSUIDataManager" ? manager : menu.loaderInfo.applicationDomain.getDefinition(name);
            }, bridge, function(message:String, failed:Boolean):void { notifications++; lastMessage = message; });
            try {
                require(subscriptions == 1 && requests.join(",") == "SettingsPanel_OpenSettings,SettingsPanel_OpenCategory:0",
                    "gameplay requests the engine settings category once");
                require(adapter.rows.length == 0, "gameplay begins empty before its first feed");
                var disabled:Object = option(0x02000002, "Disabled"); disabled.bEnabled = false;
                var checkbox:Object = {uID:0x02000003, uType:3, bIsPEO:true, bEnabled:true, sText:"Checkbox", sDescription:"",
                    checkBoxData:{bChecked:true, bOffCustomText:"Disabled", bOnCustomText:"Enabled"}};
                var entries:Array = [
                    {uType:5, sText:"Difficulty"}, option(0x00000123, "Base"), option(0x01000123, "Official expansion"),
                    {uType:5, sText:" \u00A0\u200B\uFEFF "}, option(0x02000001, "Mod choice"), disabled, checkbox,
                    option(0x03000001, "Read only")
                ];
                callback(new GameplayDataEvent({aGeneralSettingsList:entries}));
                require(adapter.rows.length == 6 && notifications == 1, "gameplay publishes one row per supported option and one refresh");
                var base:Object = adapter.rows[0], official:Object = adapter.rows[1], mod:Object = adapter.rows[2];
                require(base.mod == "gameplay/starfield" && official.mod == base.mod,
                    "official gameplay plugins share the Starfield page");
                require(base.key == "starfield.esm/123" && official.key == "official.esm/123" && base.key != official.key,
                    "official plugins with equal local form IDs retain distinct keys");
                require(base.group == official.group && base.groupTitle == "Difficulty",
                    "official options under one engine header share a group");
                require(mod.mod == "gameplay/example.esm" && mod.key == "1" && mod.gameplayID == 0x02000001,
                    "third-party gameplay identity preserves its source and full form ID");
                require(mod.groupTitle == "Example.esm" && mod.modTitle == "Example.esm",
                    "whitespace-only gameplay headers fall back to the source filename");
                require(mod.defaultKnown == false && mod.type == "enum" && mod.value == "0",
                    "gameplay rows expose live choices without inventing reset defaults");
                require(mod.options[0].label == "Easy" && mod.options[1].label.indexOf("+10%") >= 0,
                    "gameplay options preserve labels and nonzero XP rewards");
                require(mod.hint.indexOf("First line") >= 0 && /[\r\n]/.test(mod.hint) && mod.hint.indexOf("Second line") >= 0,
                    "gameplay descriptions preserve HTML line breaks");
                require(!adapter.rows[3].editable && !adapter.rows[5].editable,
                    "engine-disabled and source-read-only gameplay options cannot be edited");
                require(adapter.rows[4].value == "1" && adapter.rows[4].options[0].label == "Disabled" && adapter.rows[4].options[1].label == "Enabled",
                    "gameplay checkbox state and custom labels become enum choices");

                var count:int = notifications;
                for each (var invalid:* in [-1, 0.5, 2, NaN, Infinity, "invalid"]) {
                    require(!adapter.edit(mod, invalid), "gameplay rejects invalid choice " + String(invalid));
                }
                require(!adapter.edit(adapter.rows[3], 1) && !adapter.edit(adapter.rows[5], 1) && commits.length == 0 && notifications == count,
                    "invalid and read-only edits never reach the engine or notify success");
                allowCommit = false;
                require(!adapter.edit(mod, 1) && mod.value == "0" && notifications == count,
                    "rejected engine gameplay commits preserve the prior value");
                allowCommit = true;
                require(adapter.edit(mod, "1") && mod.value == "1" && notifications == count + 1 && lastMessage == "gameplay.saved",
                    "accepted gameplay commit updates the row and reports success");
                var commit:Object = commits[commits.length - 1];
                require(commit.id == 0x02000001 && commit.type == 1 && commit.selected == 1,
                    "gameplay commit forwards exact engine ID, type and choice");
                require(adapter.edit(adapter.rows[4], 0) && commits[commits.length - 1].type == 3 && adapter.rows[4].value == "0",
                    "gameplay checkbox edits retain the engine checkbox type");

                // The engine feed is authoritative, including changed values and
                // removed rows. Old row objects must never remain writable.
                callback(new GameplayDataEvent({aGeneralSettingsList:[{uType:5, sText:"Renamed"}, option(0x02000001, "Updated", 0)]}));
                require(adapter.rows.length == 1 && adapter.rows[0].value == "0" && adapter.rows[0].title == "Updated" && adapter.rows[0].modTitle == "Renamed",
                    "gameplay refresh replaces stale values, labels and removed options");
                var before:int = commits.length;
                require(!adapter.edit(mod, 0) && commits.length == before, "gameplay rejects stale row objects after refresh");
                callback(new GameplayDataEvent({aGeneralSettingsList:[{uType:5, sText:"First"}, option(0x02000001, "One"),
                    {uType:5, sText:"Second"}, option(0x02000002, "Two")]}));
                require(adapter.rows[0].group != adapter.rows[1].group && adapter.rows[0].modTitle == "Example.esm" && adapter.rows[1].modTitle == "Example.esm",
                    "multiple gameplay groups keep distinct section identities and the source page title");

                var nonPEO:Object = option(0x02000002, "Non-PEO"); nonPEO.bIsPEO = false;
                var unsupported:Object = option(0x02000003, "Unsupported"); unsupported.uType = 9;
                var empty:Object = option(0x02000004, "Empty"); empty.stepperData.aStepperOptions = [];
                callback(new GameplayDataEvent({aGeneralSettingsList:[option(0x02000001, "Valid"), option(0x02000001, "Duplicate"),
                    option(0x04000001, "Unknown source"), nonPEO, unsupported, empty, option(0x02000005, "Out of range", 2)]}));
                require(adapter.rows.length == 1 && adapter.rows[0].title == "Valid",
                    "gameplay ignores duplicate, unknown, non-PEO, unsupported and invalid-choice entries");
                callback(new GameplayDataEvent({aGeneralSettingsList:"invalid"}));
                require(adapter.rows.length == 0, "malformed gameplay list clears prior rows");
                callback(new GameplayDataEvent(null));
                require(adapter.rows.length == 0, "missing gameplay payload remains empty");
            } finally { adapter.dispose(); }
            require(removals == 1, "gameplay subscription is removed exactly once");
        }
    }

}
