package
{
    import flash.display.DisplayObject;
    import flash.display.DisplayObjectContainer;
    import flash.display.Loader;
    import flash.display.MovieClip;
    import flash.display.StageAlign;
    import flash.display.StageScaleMode;
    import flash.events.Event;
    import flash.events.IOErrorEvent;
    import flash.events.KeyboardEvent;
    import flash.events.UncaughtErrorEvent;
    import flash.net.URLLoader;
    import flash.net.URLRequest;
    import flash.system.ApplicationDomain;
    import flash.system.LoaderContext;
    import flash.text.Font;
    import flash.text.TextField;
    import flash.text.TextFormat;
    import flash.ui.Keyboard;

    [SWF(width="1920", height="1080", frameRate="60", backgroundColor="#071219")]
    public final class PreviewHost extends MovieClip
    {
        private var libraries:Array = [];
        private var loaders:Array = [];
        private var rows:Array = [];
        private var issues:Array = [];
        private var fontNames:Array = [];
        private var domain:ApplicationDomain;
        private var menu:Object;
        private var message:TextField = new TextField();
        private var caption:TextField = new TextField();
        private var captureState:Object = {state:"idle", keyCode:255, name:"", released:false};
        private var captureMod:String;
        private var captureKey:String;
        private var held:Object = {};
        private var nativeBindings:PreviewBindings = new PreviewBindings();

        public function PreviewHost()
        {
            stage.scaleMode = StageScaleMode.SHOW_ALL;
            stage.align = StageAlign.TOP_LEFT;
            stage.tabChildren = false;
            message.defaultTextFormat = new TextFormat("_sans", 24, 0xFFFFFF);
            message.x = 160; message.y = 20; message.width = 1600; message.height = 160;
            message.multiline = true; message.wordWrap = true; message.mouseEnabled = false;
            addChild(message); report("Loading preview assets...");
            loaderInfo.uncaughtErrorEvents.addEventListener(UncaughtErrorEvent.UNCAUGHT_ERROR, failed);
            stage.addEventListener(KeyboardEvent.KEY_DOWN, key, true, 100);
            stage.addEventListener(KeyboardEvent.KEY_UP, key, true, 100);
            // With no focused child (including a startup failure), Stage is the
            // event target and Flash does not invoke its capture listeners.
            stage.addEventListener(KeyboardEvent.KEY_DOWN, key, false, 100);
            stage.addEventListener(KeyboardEvent.KEY_UP, key, false, 100);
            caption.defaultTextFormat = new TextFormat("_sans", 18, 0x9EA9B0);
            caption.x = 160; caption.y = 1045; caption.width = 1600; caption.height = 30;
            caption.text = "RUFFLE PREVIEW    |    E / Enter: toggle / next choice    |    Arrows: adjust    |    Tab / Esc: back    |    F5: reset    |    Values stay in memory";
            caption.mouseEnabled = false; addChild(caption);
            addEventListener(Event.ENTER_FRAME, previewLabels);
            restart();
        }

        private function restart():void
        {
            if (menu) { removeChild(menu as MovieClip); menu = null; }
            for each (var loader:Loader in loaders) loader.unloadAndStop();
            loaders = []; libraries = []; fontNames = []; rows = []; issues = [];
            domain = new ApplicationDomain(ApplicationDomain.currentDomain);
            var config:URLLoader = new URLLoader();
            config.addEventListener(IOErrorEvent.IO_ERROR, ioFailed);
            config.addEventListener(Event.COMPLETE, configured);
            config.load(new URLRequest("preview.xml"));
        }

        private function configured(event:Event):void
        {
            var config:XML = new XML(URLLoader(event.target).data);
            for each (var issue:XML in config.issues.issue) {
                issues.push({type:"issue", mod:String(issue.@mod), id:String(issue.@id), modTitle:String(issue.@modTitle),
                    severity:String(issue.@severity), title:String(issue.title), reason:String(issue.reason), impact:String(issue.impact), nextSteps:String(issue.nextSteps), nexusModId:uint(issue.@nexusModId)});
            }
            for each (var library:XML in config.libraries.library) libraries.push(String(library.@url));
            for each (var font:XML in config.fonts.font) fontNames.push(String(font.@name));
            for each (var row:XML in config.rows.row) {
                if (String(row.@type) == "action") {
                    rows.push({modTitle:String(row.@modTitle), modDescription:String(row.@modDescription),
                        group:String(row.@group), groupTitle:String(row.@groupTitle), title:String(row.@title),
                        mod:String(row.@mod), key:String(row.@key), hint:String(row.@hint), type:"action",
                        editable:true, confirmation:String(row.@confirmation), actionState:String(row.@actionState),
                        message:String(row.@message)});
                    continue;
                }
                var value:*;
                if (String(row.@type) == "float" || String(row.@type) == "key") value = Number(row.@value);
                else if (String(row.@type) == "int" || String(row.@type) == "enum" || String(row.@type) == "string") value = String(row.@value);
                else value = String(row.@value) == "true";
                var choices:Array = [];
                for each (var option:XML in row.option) choices.push({value:String(option.@value), label:String(option.@label)});
                rows.push({modTitle:String(row.@modTitle), modDescription:String(row.@modDescription),
                    group:String(row.@group), groupTitle:String(row.@groupTitle), title:String(row.@title),
                    mod:String(row.@mod), key:String(row.@key), hint:String(row.@hint),
                    requiresRestart:String(row.@requiresRestart) == "true", maxLength:int(row.@maxLength),
                    type:String(row.@type), editable:String(row.@editable) == "true", allowUnbound:String(row.@allowUnbound) == "true",
                    minimum:String(row.@minimum), maximum:String(row.@maximum), value:value, defaultValue:value, options:choices,
                    valueName:String(row.@type) == "key" ? previewKeyName(uint(value)) : "",
                    defaultName:String(row.@type) == "key" ? previewKeyName(uint(value)) : "",
                    decimals:int(row.@decimals), sliderMinimum:Number(row.@sliderMinimum), sliderMaximum:Number(row.@sliderMaximum),
                    sliderStep:Number(row.@sliderStep), sliderScale:Number(row.@sliderScale), sliderSteps:Number(row.@sliderSteps)});
            }
            loadNext();
        }

        private function loadNext(event:Event = null):void
        {
            if (event && String(event.target.url).indexOf("fonts_en.swf") >= 0) {
                for each (var alias:String in fontNames) {
                    Font.registerFont(event.target.applicationDomain.getDefinition(alias) as Class);
                }
            }
            if (libraries.length == 0) { loadMenu(); return; }
            var url:String = libraries.shift();
            report("Loading " + url);
            var loader:Loader = new Loader(); loaders.push(loader);
            loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, ioFailed);
            loader.contentLoaderInfo.addEventListener(Event.COMPLETE, loadNext);
            loader.contentLoaderInfo.uncaughtErrorEvents.addEventListener(UncaughtErrorEvent.UNCAUGHT_ERROR, failed);
            // Shared font resources need not expose their AS3 classes to menus.
            var libraryDomain:ApplicationDomain = url.indexOf("fonts_en.swf") >= 0 ? new ApplicationDomain(domain) : domain;
            loader.load(new URLRequest(url), new LoaderContext(false, libraryDomain));
        }

        private function loadMenu():void
        {
            if (domain.hasDefinition("$MAIN_Font_Bold") || domain.hasDefinition("$NB_Grotesk_Semibold")) {
                throw new Error("Preview font classes leaked into the menu domain");
            }
            var manager:Class = domain.getDefinition("Shared.AS3.Data.BSUIDataManager") as Class;
            var controls:Object = Object(manager).GetDataFromClient("ControlMapData");
            controls.data.uiController = 0;
            controls.data.vMappedEvents = [
                {strUserEventName:"Accept", strButtonName:"E", aButtonName:["E"], sContextName:"BasicMenuNav"},
                {strUserEventName:"Cancel", strButtonName:"Tab", aButtonName:["Tab"], sContextName:"BasicMenuNav"},
                {strUserEventName:"YButton", strButtonName:"B", aButtonName:["B"], sContextName:"BasicMenuNav"},
                {strUserEventName:"XButton", strButtonName:"X", aButtonName:["X"], sContextName:"BasicMenuNav"},
                {strUserEventName:"LShoulder", strButtonName:"[", aButtonName:["["], sContextName:"BasicMenuNav"},
                {strUserEventName:"RShoulder", strButtonName:"]", aButtonName:["]"], sContextName:"BasicMenuNav"}
            ];
            controls.SetReady(true);
            var bindings:Object = Object(manager).GetDataFromClient("ControlBindingsData");
            bindings.data.aInputSettingsList = nativeBindings.entries;
            bindings.data.bShowSecondaryBindings = true;
            bindings.data.bRemappingControl = false;
            bindings.SetReady(true);
            var loader:Loader = new Loader(); loaders.push(loader);
            loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, ioFailed);
            loader.contentLoaderInfo.addEventListener(Event.COMPLETE, menuLoaded);
            loader.contentLoaderInfo.uncaughtErrorEvents.addEventListener(UncaughtErrorEvent.UNCAUGHT_ERROR, failed);
            loader.load(new URLRequest("menu.swf"), new LoaderContext(false, domain));
        }

        private function menuLoaded(event:Event):void
        {
            menu = event.target.content;
            menu.BGSCodeObj = {getRows:getRows, getIssues:getIssues, openIssueModPage:openIssueModPage, setBool:setBool, setInt:setInt, setFloat:setFloat, setEnum:setEnum, setString:setString, close:closeMenu,
                revision:function():String { return "0:0:0"; }, invokeAction:invokeAction,
                launch:function(mod:String, id:String):Boolean {
                    trace("[preview] launch " + mod + "/" + id);
                    return false; // The preview cannot open engine/provider menus.
                },
                startup:startup, startupFailed:report, setKey:setKey,
                beginKeyCapture:beginKeyCapture, pollKeyCapture:function():Object { return captureState; },
                commitKeyCapture:commitKeyCapture, cancelKeyCapture:function():void { captureState.state = "idle"; },
                previewKey:captureButton,
                previewBindingTitle:function(entry:Object):String { return nativeBindings.labels[entry.sInputName] || entry.sInputName; },
                previewBindingCell:fitNativeBindingCell,
                previewConstruct:function(name:String, clip:Object):void {
                    // Ruffle rejects the game's class-only placement in this popup.
                    // Restore that authored child from its real game symbol.
                    if (name == "RemapConfirmation" && !clip.ButtonBar_mc.CancelButton_mc) {
                        var button:Class = domain.getDefinition("BasicButton") as Class;
                        clip.ButtonBar_mc.CancelButton_mc = new button();
                        clip.ButtonBar_mc.addChild(clip.ButtonBar_mc.CancelButton_mc);
                    }
                },
                requestBindings:function():uint { return ++nativeBindings.generation; },
                pollBindings:nativeBindings.snapshot,
                textInput:function(enabled:Boolean):Boolean { return true; },
                beginNativeBinding:function():Boolean { return false; },
                endNativeBinding:function(cancel:Boolean):void {}};
            addChild(menu as MovieClip);
            menu.onCodeObjCreate();
            // Fixtures enter through the same native data publication and schema join.
            // Keep ordinary design pages unchanged until Keybindings is selected.
            setChildIndex(message, numChildren - 1);
            setChildIndex(caption, numChildren - 1);
            if (loaderInfo.parameters.verify == "true") new PreviewChecks(menu as MovieClip,
                loaderInfo.parameters.verifyBindings == "true", loaderInfo.parameters.verifyCaptures == "true");
        }

        private function getRows():Array
        {
            // Return new row objects like the native bridge. Changes are kept
            // in memory here; preview never reads or writes the player's values.
            var result:Array = [];
            for each (var row:Object in rows) {
                var copy:Object = {};
                for (var property:String in row) copy[property] = row[property];
                result.push(copy);
            }
            return result.concat(nativeBindings.definitions, launcherRows());
        }

        private function launcherRows():Array
        {
            // Launcher-only mods also exercise separation from the settings browser.
            var names:Array = ["Absolute Control", "Character Studio", "OSF Animation Browser", "DevilzDad's Shop + Explorer",
                "Starcade OS", "AISS Companion Log", "Camera Tools", "An interface with a deliberately long display title", "Ship Planner", "Unavailable Interface"];
            var result:Array = [];
            for (var i:int = 0; i < names.length; ++i) result.push({type:"launcher", mod:"launcher-preview-" + i,
                modTitle:names[i], group:"@launcher", groupTitle:"Launcher", key:"open", title:names[i],
                hint:"Open this mod's interface.", recentOrder:i < 4 ? 4 - i : 0,
                editable:i != names.length - 1, message:i == names.length - 1 ? "This interface is unavailable in the current context." : ""});
            return result;
        }

        private function fitNativeBindingCell(cell:Object):void
        {
            // ControlBinding requests Scaleform TextFieldEx.TEXTAUTOSZ_SHRINK.
            // Ruffle does not implement that native sizing. Approximate it only
            // in preview; production keeps the game's field/glyph implementation.
            for each (var field:TextField in [cell.PCKey_mc.PCKey_tf,cell.Icon_mc.Icon_tf]) {
                var format:TextFormat = field.defaultTextFormat;
                field.setTextFormat(format);
                while (Number(format.size) > 10 && field.textWidth > field.width - 4) {
                    format.size = Number(format.size) - 1; field.setTextFormat(format);
                }
            }
        }

        private function openIssueModPage(mod:String, id:String):Boolean
        {
            trace("Preview: open mod page for " + mod + "/" + id);
            return true;
        }

        private function getIssues():Array
        {
            var result:Array = [];
            for each (var issue:Object in issues) {
                var copy:Object = {};
                for (var key:String in issue) copy[key] = issue[key];
                result.push(copy);
            }
            return result;
        }

        private function invokeAction(mod:String, key:String):Object
        {
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "action") {
                    row.actionState = "Completed";
                    row.message = "Preview action completed.";
                    return {ok:true};
                }
            }
            return {ok:false, error:"Unknown preview action."};
        }

        private function setBool(mod:String, key:String, value:Boolean):Object
        {
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "bool") {
                    row.value = value; trace("[preview] " + mod + "/" + key + " = " + value);
                    return {ok:true, error:""};
                }
            }
            return {ok:false, error:"Unknown preview setting."};
        }

        private function setInt(mod:String, key:String, value:String):Object
        {
            var number:Number = Number(value);
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "int" && row.editable &&
                    /^-?\d+$/.test(value) && isFinite(number) && number == Math.round(number) &&
                    number >= Number(row.minimum) && number <= Number(row.maximum)) {
                    row.value = String(number); trace("[preview] " + mod + "/" + key + " = " + row.value);
                    return {ok:true, error:""};
                }
            }
            return {ok:false, error:"Invalid preview integer setting."};
        }

        private function setFloat(mod:String, key:String, value:Number):Object
        {
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "float" && isFinite(value) &&
                    (row.minimum == "" || value >= Number(row.minimum)) && (row.maximum == "" || value <= Number(row.maximum))) {
                    row.value = value; trace("[preview] " + mod + "/" + key + " = " + value);
                    return {ok:true, error:""};
                }
            }
            return {ok:false, error:"Invalid preview float setting."};
        }

        private function setEnum(mod:String, key:String, value:String):Object
        {
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "enum") {
                    for each (var option:Object in row.options) {
                        if (option.value === value) {
                            row.value = value; trace("[preview] " + mod + "/" + key + " = " + value);
                            return {ok:true, error:""};
                        }
                    }
                }
            }
            return {ok:false, error:"Invalid preview enum setting."};
        }

        private function setKey(mod:String, key:String, value:Number):Object
        {
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "key" && isFinite(value) && value == Math.floor(value) && (value == 255 ? row.allowUnbound : bindableKey(value))) {
                    row.value = value; row.valueName = previewKeyName(uint(value)); return {ok:true};
                }
            }
            return {ok:false, error:"Invalid preview key setting."};
        }
        private function setString(mod:String, key:String, value:String, bytes:int):Object
        {
            // The loaded menu owns the same text validation as the in-game editor.
            var strings:Class = domain.getDefinition("StringSetting") as Class;
            var actual:int = Object(strings).byteLength(value);
            for each (var row:Object in rows) {
                if (row.mod == mod && row.key == key && row.type == "string" && actual >= 0 && actual == bytes && actual <= row.maxLength) {
                    row.value = value; return {ok:true};
                }
            }
            return {ok:false, error:"Invalid preview string setting."};
        }
        private function beginKeyCapture(mod:String, key:String):Object
        {
            captureMod = mod; captureKey = key;
            captureState = {state:"waiting", keyCode:255, name:"", released:false};
            return {ok:true};
        }
        private function commitKeyCapture():Object
        {
            if (!captureState.released || (captureState.state != "candidate" && captureState.state != "confirmed")) return {ok:false};
            var result:Object = menu.BGSCodeObj.setKey(captureMod, captureKey, captureState.keyCode);
            captureState.state = result.ok ? "idle" : "candidate";
            return result;
        }
        private function bindableKey(code:Number):Boolean
        {
            return code > 0 && code < 255 && code != 1 && code != 2 && code != 4 && code != 5 && code != 6 && code != 27;
        }
        private function captureButton(code:uint, down:Boolean):void
        {
            var repeat:Boolean = Boolean(held[code]); held[code] = down;
            if (captureState.state == "idle") return;
            if (!down) { if (code == captureState.keyCode) captureState.released = true; return; }
            if (repeat) return;
            if (code == 27) captureState.state = "cancelled";
            else if (captureState.state == "waiting" && bindableKey(code)) {
                captureState.keyCode = code; captureState.name = previewKeyName(code); captureState.state = "candidate";
            }
            else if (captureState.state == "candidate" && captureState.released && code == 13) captureState.state = "confirmed";
        }
        // Adapt Flash modifier locations to Starfield's Win32 virtual-key identities.
        private function previewKeyCode(event:KeyboardEvent):uint
        {
            if (event.keyCode == 16) return event.keyLocation == 2 ? 161 : 160;
            if (event.keyCode == 17) return event.keyLocation == 2 ? 163 : 162;
            if (event.keyCode == 18) return event.keyLocation == 2 ? 165 : 164;
            return event.keyCode;
        }
        // Preview labels only; native labels come from Windows. This never gates capture.
        private function previewKeyName(code:uint):String
        {
            if (code >= 65 && code <= 90 || code >= 48 && code <= 57) return String.fromCharCode(code);
            if (code >= 112 && code <= 135) return "F" + (code - 111);
            if (code >= 96 && code <= 105) return "Numpad" + (code - 96);
            var names:Object = {8:"Backspace",9:"Tab",13:"Enter",19:"Pause",20:"CapsLock",27:"Escape",32:"Space",
                160:"Left Shift",161:"Right Shift",162:"Left Ctrl",163:"Right Ctrl",164:"Left Alt",165:"Right Alt",255:"UNBOUND"};
            return names[code] || "Key 0x" + code.toString(16).toUpperCase();
        }
        private function startup(phase:String):void
        {
            trace("[preview] " + phase);
            if (phase == "ready") message.visible = false;
        }

        private function closeMenu():void
        {
            removeChild(menu as MovieClip); menu = null;
            report("Menu closed. Press F5 to reopen with schema defaults.");
        }

        private function key(event:KeyboardEvent):void
        {
            if (stage.focus is TextField && TextField(stage.focus).type == "input") return;
            var binding:uint = previewKeyCode(event);
            if (captureState.state != "idle" || held[binding]) {
                captureButton(binding, event.type == KeyboardEvent.KEY_DOWN);
                event.stopImmediatePropagation(); event.preventDefault(); return;
            }
            if (event.keyCode == Keyboard.F5) {
                event.stopImmediatePropagation(); event.preventDefault();
                if (event.type == KeyboardEvent.KEY_UP) restart();
                return;
            }
            var name:String = event.keyCode == Keyboard.ENTER || event.keyCode == Keyboard.E ? "Accept" :
                event.keyCode == Keyboard.TAB || event.keyCode == Keyboard.ESCAPE ? "Cancel" : "";
            if (name && menu) {
                event.stopImmediatePropagation(); event.preventDefault();
                menu.ProcessUserEvent(name, event.type == KeyboardEvent.KEY_DOWN);
                // Opening Accept has already been delivered. Ignore its repeats until release.
                if (captureState.state == "waiting" && event.type == KeyboardEvent.KEY_DOWN) held[binding] = true;
            }
        }

        private function previewLabels(event:Event):void
        {
            if (menu) translate(menu as DisplayObject);
        }
        private function translate(child:DisplayObject):void
        {
            // The game translates these English labels; Flash has no translator.
            var text:TextField = child as TextField;
            if (text && text.text.indexOf("$MainGameplay_") >= 0) {
                var token:String = text.text.replace(/^\$+MainGameplay_/,"").replace(/_KBM$/,"").replace(/ \*$/,"");
                if (nativeBindings.labels[token]) text.text = nativeBindings.labels[token];
            }
            if (text && (text.text == "$BACK" || text.text == "$ON" || text.text == "$OFF")) {
                var format:TextFormat = text.getTextFormat();
                text.text = text.text.substr(1); text.setTextFormat(format);
            }
            var container:DisplayObjectContainer = child as DisplayObjectContainer;
            if (container) {
                for (var i:int = 0; i < container.numChildren; ++i) translate(container.getChildAt(i));
            }
        }

        private function report(text:String):void
        {
            trace("[preview] " + text); message.text = text; message.visible = true;
        }
        private function ioFailed(event:IOErrorEvent):void { report(event.text); }
        private function failed(event:UncaughtErrorEvent):void
        {
            event.preventDefault(); report("Preview error: " + event.error);
        }
    }
}
