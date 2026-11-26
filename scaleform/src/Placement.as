package
{
    import flash.display.Graphics;

    // Geometry shared by placement rows, their thumbnails and the editor. A placement owns no
    // value: its rectangle is read from, and saved to, the int and float rows it binds.
    // Rectangles are {left, top, width, height} in the placement's screen units.
    public final class Placement
    {
        public static const ROLES:Array = ["x", "y", "left", "top", "right", "bottom", "width", "height", "scale"];
        private static const FIELDS:Object = {x:"bindX", y:"bindY", left:"bindLeft", top:"bindTop", right:"bindRight",
            bottom:"bindBottom", width:"bindWidth", height:"bindHeight", scale:"bindScale"};

        // Links every placement row to the rows it binds and gives it the value, default and
        // text the list shows. Run after each getRows(), since saves replace row objects.
        public static function annotate(rows:Array):void
        {
            var numeric:Object = {};
            for each (var row:Object in rows) {
                if (row.type != "int" && row.type != "float") continue;
                if (!numeric[row.mod]) numeric[row.mod] = {};
                numeric[row.mod][row.key] = row;
            }
            for each (row in rows) {
                if (row.type != "placement") continue;
                var settings:Object = numeric[row.mod] || {};
                var bound:Object = {}, missing:Boolean = false, count:int = 0;
                for each (var role:String in ROLES) {
                    var key:* = row[FIELDS[role]];
                    if (key == null || key === "") continue;
                    var target:Object = settings.hasOwnProperty(key) ? settings[key] : null;
                    if (!target) { missing = true; continue; }
                    bound[role] = target; ++count;
                }
                row.bound = bound;
                row.editable = row.editable !== false && !missing && count > 0;
                row.rect = rect(row, current(row));
                row.defaultRect = rect(row, defaults(row));
                // Plain strings, so the list's changed marker compares them like any other value.
                row.value = signature(row, current(row));
                row.defaultValue = signature(row, defaults(row));
                row.valueText = describe(row, row.rect);
                row.defaultText = describe(row, row.defaultRect);
            }
        }

        public static function has(row:Object, role:String):Boolean { return Boolean(row.bound && row.bound[role]); }
        public static function screenWidth(row:Object):Number { return Number(row.screenWidth) || 1920; }
        public static function screenHeight(row:Object):Number { return Number(row.screenHeight) || 1080; }

        // The saved values, and the defaults, of the bound rows by role.
        public static function current(row:Object):Object { return read(row, false); }
        public static function defaults(row:Object):Object { return read(row, true); }
        private static function read(row:Object, useDefaults:Boolean):Object
        {
            var values:Object = {};
            for (var role:String in row.bound) values[role] = Number(useDefaults ? row.bound[role].defaultValue : row.bound[role].value);
            return values;
        }

        // An axis can save a size when its size or the scale is bound, or both of its margins are.
        public static function sizable(row:Object, horizontal:Boolean):Boolean
        {
            if (String(row.resize) == "none") return false;
            if (has(row, "scale")) return true;
            return horizontal ? has(row, "width") || has(row, "left") && has(row, "right") :
                has(row, "height") || has(row, "top") && has(row, "bottom");
        }
        // An axis without a position binding stays where the rectangle is drawn.
        public static function movable(row:Object, horizontal:Boolean):Boolean
        {
            return horizontal ? has(row, "x") || has(row, "left") || has(row, "right") : has(row, "y") || has(row, "top") || has(row, "bottom");
        }
        public static function proportional(row:Object):Boolean { return String(row.resize) == "scale"; }

        // The rectangle a set of role values describes.
        public static function rect(row:Object, values:Object):Object
        {
            var w:Number = size(row, values, true), h:Number = size(row, values, false);
            return {left:start(row, values, true, w), top:start(row, values, false, h), width:w, height:h};
        }
        private static function size(row:Object, values:Object, horizontal:Boolean):Number
        {
            var screen:Number = horizontal ? screenWidth(row) : screenHeight(row);
            var base:Number = Number(horizontal ? row.sizeWidth : row.sizeHeight);
            var sizeRole:String = horizontal ? "width" : "height", first:String = horizontal ? "left" : "top", last:String = horizontal ? "right" : "bottom";
            var value:Number = NaN;
            if (values.hasOwnProperty(sizeRole)) value = values[sizeRole];
            else if (values.hasOwnProperty("scale")) value = base * values.scale;
            else if (values.hasOwnProperty(first) && values.hasOwnProperty(last)) value = screen - values[first] - values[last];
            else value = base;
            if (isFinite(value) && value > 0) return value;
            return base > 0 ? base : screen / 4;
        }
        private static function start(row:Object, values:Object, horizontal:Boolean, extent:Number):Number
        {
            var screen:Number = horizontal ? screenWidth(row) : screenHeight(row);
            var point:String = horizontal ? "x" : "y", first:String = horizontal ? "left" : "top", last:String = horizontal ? "right" : "bottom";
            var origin:Number = Number(horizontal ? row.originX : row.originY) || 0;
            var value:Number = NaN;
            if (values.hasOwnProperty(point)) value = values[point] - origin * extent;
            else if (values.hasOwnProperty(first)) value = values[first];
            else if (values.hasOwnProperty(last)) value = screen - values[last] - extent;
            return isFinite(value) ? value : (screen - extent) / 2;
        }

        // Every role's value for a rectangle, bound or not.
        public static function measure(row:Object, r:Object):Object
        {
            var ox:Number = Number(row.originX) || 0, oy:Number = Number(row.originY) || 0;
            return {x:r.left + ox * r.width, y:r.top + oy * r.height, left:r.left, top:r.top,
                right:screenWidth(row) - r.left - r.width, bottom:screenHeight(row) - r.top - r.height,
                width:r.width, height:r.height, scale:Number(row.sizeWidth) > 0 ? r.width / Number(row.sizeWidth) : 1};
        }
        // The values a save would write, rounded the way each bound setting stores them.
        public static function values(row:Object, r:Object):Object
        {
            var measured:Object = measure(row, r), result:Object = {};
            for (var role:String in row.bound) result[role] = round(row.bound[role], measured[role]);
            return result;
        }
        public static function round(target:Object, value:Number):Number
        {
            if (target.type == "int") return Math.round(value);
            var decimals:int = int(target.decimals) >= 0 ? int(target.decimals) : 3;
            var factor:Number = Math.pow(10, Math.min(decimals, 6));
            return Math.round(value * factor) / factor;
        }
        // The rectangle exactly as it would be saved and read back.
        public static function settle(row:Object, r:Object):Object
        {
            var settled:Object = rect(row, values(row, r));
            // Unbound axes are not saved, so they keep the drawn position and size.
            if (!movable(row, true)) settled.left = r.left;
            if (!movable(row, false)) settled.top = r.top;
            return settled;
        }

        // The inclusive range of a bound setting, or null.
        public static function range(row:Object, role:String):Object
        {
            var target:Object = row.bound ? row.bound[role] : null;
            if (!target) return null;
            var low:Number = target.minimum === undefined || target.minimum === "" ? -Infinity : Number(target.minimum);
            var high:Number = target.maximum === undefined || target.maximum === "" ? Infinity : Number(target.maximum);
            if (isNaN(low)) low = -Infinity;
            if (isNaN(high)) high = Infinity;
            return {low:low, high:high};
        }
        // Sizes the bound settings allow, never smaller than a few units or larger than the screen.
        public static function sizeLimits(row:Object):Object
        {
            var W:Number = screenWidth(row), H:Number = screenHeight(row);
            var limits:Object = {minWidth:Math.max(4, W / 200), maxWidth:W, minHeight:Math.max(4, H / 200), maxHeight:H};
            var span:Object = range(row, "width");
            if (span) { limits.minWidth = Math.max(limits.minWidth, span.low); limits.maxWidth = Math.min(limits.maxWidth, span.high); }
            span = range(row, "height");
            if (span) { limits.minHeight = Math.max(limits.minHeight, span.low); limits.maxHeight = Math.min(limits.maxHeight, span.high); }
            span = range(row, "scale");
            var baseWidth:Number = Number(row.sizeWidth), baseHeight:Number = Number(row.sizeHeight);
            if (span && baseWidth > 0 && baseHeight > 0) {
                limits.minWidth = Math.max(limits.minWidth, span.low * baseWidth); limits.maxWidth = Math.min(limits.maxWidth, span.high * baseWidth);
                limits.minHeight = Math.max(limits.minHeight, span.low * baseHeight); limits.maxHeight = Math.min(limits.maxHeight, span.high * baseHeight);
            }
            return limits;
        }
        // Left (or top) edges that keep the rectangle on screen and every bound position in range.
        public static function startLimits(row:Object, horizontal:Boolean, extent:Number):Object
        {
            var screen:Number = horizontal ? screenWidth(row) : screenHeight(row);
            var origin:Number = Number(horizontal ? row.originX : row.originY) || 0;
            var low:Number = 0, high:Number = screen - extent;
            var span:Object = range(row, horizontal ? "x" : "y");
            if (span) { low = Math.max(low, span.low - origin * extent); high = Math.min(high, span.high - origin * extent); }
            span = range(row, horizontal ? "left" : "top");
            if (span) { low = Math.max(low, span.low); high = Math.min(high, span.high); }
            span = range(row, horizontal ? "right" : "bottom");
            if (span) { low = Math.max(low, screen - extent - span.high); high = Math.min(high, screen - extent - span.low); }
            return {low:low, high:high};
        }
        // Whether a rectangle of this size has any allowed position.
        public static function fits(row:Object, width:Number, height:Number):Boolean
        {
            var limits:Object = sizeLimits(row);
            if (width < limits.minWidth - 0.001 || width > limits.maxWidth + 0.001 || height < limits.minHeight - 0.001 || height > limits.maxHeight + 0.001) return false;
            var across:Object = startLimits(row, true, width), down:Object = startLimits(row, false, height);
            return across.low <= across.high + 0.001 && down.low <= down.high + 0.001;
        }
        // Moves a rectangle the least distance into its allowed positions.
        public static function confine(row:Object, r:Object):Object
        {
            var result:Object = {left:r.left, top:r.top, width:r.width, height:r.height};
            if (movable(row, true)) {
                var across:Object = startLimits(row, true, r.width);
                if (across.low <= across.high) result.left = Math.max(across.low, Math.min(across.high, r.left));
            }
            if (movable(row, false)) {
                var down:Object = startLimits(row, false, r.height);
                if (down.low <= down.high) result.top = Math.max(down.low, Math.min(down.high, r.top));
            }
            return result;
        }
        // Where keyboard and wheel resizing keep the rectangle fixed: the point the mod positions it by.
        public static function anchor(row:Object, horizontal:Boolean):Number
        {
            if (has(row, horizontal ? "x" : "y")) return Number(horizontal ? row.originX : row.originY) || 0;
            if (has(row, horizontal ? "left" : "top")) return has(row, horizontal ? "right" : "bottom") ? 0.5 : 0;
            if (has(row, horizontal ? "right" : "bottom")) return 1;
            return 0.5;
        }

        public static function number(value:Number, decimals:int = 0):String
        {
            if (!isFinite(value)) return "-";
            var text:String = value.toFixed(decimals);
            return text == "-0" ? "0" : text.indexOf(".") >= 0 ? text.replace(/\.?0+$/, "") : text;
        }
        public static function scaleText(row:Object, r:Object):String
        {
            return number(measure(row, r).scale, 2) + "x";
        }
        // "760, 24 | 400 x 120": the left and top edges, then the size.
        public static function describe(row:Object, r:Object):String
        {
            var text:String = number(r.left) + ", " + number(r.top) + " | " + number(r.width) + " x " + number(r.height);
            if (has(row, "scale")) text += " | " + scaleText(row, r);
            return text;
        }
        private static function signature(row:Object, values:Object):String
        {
            var parts:Array = [];
            for each (var role:String in ROLES) if (values.hasOwnProperty(role)) parts.push(role + "=" + values[role]);
            return parts.join(";");
        }

        // A small screen with the rectangle on it, for list rows.
        public static function thumbnail(g:Graphics, x:Number, y:Number, height:Number, row:Object, selected:Boolean):Number
        {
            var W:Number = screenWidth(row), H:Number = screenHeight(row);
            var width:Number = Math.round(height * W / H);
            g.lineStyle(1, selected ? MenuStyle.INK : MenuStyle.LINE);
            g.beginFill(selected ? 0xD9DDD5 : MenuStyle.INK); g.drawRect(x, y, width, height); g.endFill();
            var r:Object = row.rect;
            if (r) {
                var scale:Number = width / W;
                var left:Number = Math.max(0, Math.min(width - 2, r.left * scale)), top:Number = Math.max(0, Math.min(height - 2, r.top * scale));
                var w:Number = Math.max(2, Math.min(width - left, r.width * scale)), h:Number = Math.max(2, Math.min(height - top, r.height * scale));
                g.lineStyle();
                g.beginFill(MenuStyle.ACCENT); g.drawRect(x + left, y + top, w, h); g.endFill();
            }
            g.lineStyle();
            return width;
        }
    }
}
