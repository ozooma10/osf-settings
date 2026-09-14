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
        private var fontNames:Array = [];
        private var domain:ApplicationDomain;
        private var menu:Object;
        private var message:TextField = new TextField();
        private var caption:TextField = new TextField();

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
            caption.defaultTextFormat = new TextFormat("_sans", 18, 0x9EA9B0);
            caption.x = 160; caption.y = 1045; caption.width = 1600; caption.height = 30;
            caption.text = "RUFFLE PREVIEW    |    E / Enter: toggle    |    Tab / Esc: back    |    F5: reset    |    Values stay in memory";
            caption.mouseEnabled = false; addChild(caption);
            addEventListener(Event.ENTER_FRAME, previewLabels);
            restart();
        }

        private function restart():void
        {
            if (menu) { removeChild(menu as MovieClip); menu = null; }
            for each (var loader:Loader in loaders) loader.unloadAndStop();
            loaders = []; libraries = []; fontNames = []; rows = [];
            domain = new ApplicationDomain(ApplicationDomain.currentDomain);
            var config:URLLoader = new URLLoader();
            config.addEventListener(IOErrorEvent.IO_ERROR, ioFailed);
            config.addEventListener(Event.COMPLETE, configured);
            config.load(new URLRequest("preview.xml"));
        }

        private function configured(event:Event):void
        {
            var config:XML = new XML(URLLoader(event.target).data);
            for each (var library:XML in config.libraries.library) libraries.push(String(library.@url));
            for each (var font:XML in config.fonts.font) fontNames.push(String(font.@name));
            for each (var row:XML in config.rows.row) {
                rows.push({header:String(row.@header) == "true", title:String(row.@title),
                    mod:String(row.@mod), key:String(row.@key), hint:String(row.@hint),
                    value:String(row.@value) == "true", defaultValue:String(row.@value) == "true"});
            }
            loadNext();
        }

        private function loadNext(event:Event = null):void
        {
            if (event && String(event.target.url).indexOf("fonts_en.swf") >= 0) {
                for each (var alias:String in fontNames) {
                    Font.registerFont(domain.getDefinition(alias) as Class);
                }
            }
            if (libraries.length == 0) { loadMenu(); return; }
            var url:String = libraries.shift();
            report("Loading " + url);
            var loader:Loader = new Loader(); loaders.push(loader);
            loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, ioFailed);
            loader.contentLoaderInfo.addEventListener(Event.COMPLETE, loadNext);
            loader.contentLoaderInfo.uncaughtErrorEvents.addEventListener(UncaughtErrorEvent.UNCAUGHT_ERROR, failed);
            loader.load(new URLRequest(url), new LoaderContext(false, domain));
        }

        private function loadMenu():void
        {
            var manager:Class = domain.getDefinition("Shared.AS3.Data.BSUIDataManager") as Class;
            var controls:Object = Object(manager).GetDataFromClient("ControlMapData");
            controls.data.uiController = 0;
            controls.data.vMappedEvents = [
                {strUserEventName:"Accept", strButtonName:"E", aButtonName:["E"], sContextName:"BasicMenuNav"},
                {strUserEventName:"Cancel", strButtonName:"Tab", aButtonName:["Tab"], sContextName:"BasicMenuNav"}
            ];
            controls.SetReady(true);
            var loader:Loader = new Loader(); loaders.push(loader);
            loader.contentLoaderInfo.addEventListener(IOErrorEvent.IO_ERROR, ioFailed);
            loader.contentLoaderInfo.addEventListener(Event.COMPLETE, menuLoaded);
            loader.contentLoaderInfo.uncaughtErrorEvents.addEventListener(UncaughtErrorEvent.UNCAUGHT_ERROR, failed);
            loader.load(new URLRequest("menu.swf"), new LoaderContext(false, domain));
        }

        private function menuLoaded(event:Event):void
        {
            menu = event.target.content;
            menu.BGSCodeObj = {getRows:getRows, setBool:setBool, close:closeMenu,
                startup:startup, startupFailed:report};
            addChild(menu as MovieClip);
            menu.onCodeObjCreate();
            setChildIndex(message, numChildren - 1);
            setChildIndex(caption, numChildren - 1);
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
            return result;
        }

        private function setBool(mod:String, key:String, value:Boolean):Object
        {
            for each (var row:Object in rows) {
                if (!row.header && row.mod == mod && row.key == key) {
                    row.value = value; trace("[preview] " + mod + "/" + key + " = " + value);
                    return {ok:true, error:""};
                }
            }
            return {ok:false, error:"Unknown preview setting."};
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
