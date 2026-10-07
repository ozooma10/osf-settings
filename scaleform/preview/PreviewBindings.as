package
{
    public final class PreviewBindings
    {
        public var entries:Array = [];
        public var records:Array = [];
        public var definitions:Array = [];
        public var labels:Object = {};
        public var generation:uint = 0;
        public var controllerEntries:Array = [];
        public function PreviewBindings()
        {
            add("Jump","Jump",32,255,0,0,true);
            add("Forward","Move forward",87);
            add("Attack","Attack",0,255,1);
            add("Activate","Activate",69);
            add("preview/map","Open navigation overlay",77,255,0,0,false,"Navigation Tools");
            add("preview/photo","Photo capture",77,255,0,0,false,"Camera Tools");
            add("preview/chord","Capture with a modifier",77,162,0,0,false,"Camera Tools");
            add("preview/shared","Game and mod shared space",32,255,0,0,false,"Navigation Tools");
            add("preview/unbound","Unbound action",255,255,0,0,false,"Navigation Tools");
            add("preview/long","An intentionally long action label that should fit without colliding with either native binding slot",103,255,0,0,false,"Navigation Tools");
            add("preview/unsupported","Unsupported keyboard media key",179,255,0,0,false,"Camera Tools");
            add("preview/alternate","Alternate slot and duplicate slot ownership",118,255,0,0,false,"Camera Tools");
            records.push({action:"preview/alternate",context:0,device:0,slot:1,key:118,modifier:255});
            entries[entries.length - 1].AltBinding = binding(118,255,0);
            for (var i:int = 0; i < 28; ++i) add("Fixture" + i,"Additional gameplay action " + (i + 1),i < 10 ? 48 + i : 255);
            records.push({action:"CameraPath",context:0,device:0,slot:0,key:123,modifier:255,visibleInControls:false});
            for each (var entry:Object in entries) {
                var controller:Object = {};
                for (var property:String in entry) controller[property] = entry[property];
                controller.bGamepadEntry = true;
                var code:uint = entry.sInputName == "Jump" || entry.sInputName == "preview/shared" || entry.sInputName == "preview/chord" ? 4096 :
                    entry.sInputName == "Attack" ? 10 : 255;
                var modifier:uint = entry.sInputName == "preview/chord" ? 256 : 255;
                controller.MainBinding = {aPCKeyName:[], aButtonName:code == 4096 ?
                    (modifier == 256 ? ["Xenon_L1", "Xenon_A"] : ["Xenon_A"]) : code == 10 ? ["Xenon_R2"] : []};
                controller.AltBinding = {aPCKeyName:[], aButtonName:[]};
                controllerEntries.push(controller);
                records.push({action:entry.sInputName, context:0, device:2, slot:0, key:code, modifier:modifier});
            }
        }
        private function binding(key:uint,modifier:uint,device:uint):Object
        {
            var names:Array = [];
            if (key != 255) {
                if (modifier != 255) names.push("L Ctrl");
                names.push(device == 1 ? "Mouse " + (key + 1) : key == 32 ? "Space" : key == 118 ? "F7" : key == 103 ? "Numpad 7" : key == 179 ? "Media Play" : String.fromCharCode(key));
            }
            return {aButtonName:[],aPCKeyName:names};
        }
        private function add(action:String,title:String,key:uint,modifier:uint = 255,device:uint = 0,slot:uint = 0,required:Boolean = false,mod:String = ""):void
        {
            entries.push({sInputName:action,sContextName:"MainGameplay",uContextID:0,bIsDivider:false,bGamepadEntry:false,
                bReadOnly:false,bRequired:required,MainBinding:binding(key,modifier,device),AltBinding:binding(255,255,0)});
            records.push({action:action,context:0,device:device,slot:slot,key:key,modifier:modifier});
            labels[action] = title;
            if (mod) definitions.push({type:"hotkey",registered:true,action:action,mod:mod == "Camera Tools" ? "camera" : "navigation",modTitle:mod,
                group:"hotkeys",groupTitle:"Hotkeys",key:action,title:title,defaultName:"Unbound"});
        }
        public function snapshot():Object { return {generation:generation,state:"ready",records:records}; }
    }
}
