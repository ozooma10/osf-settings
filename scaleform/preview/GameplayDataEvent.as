package
{
    import flash.events.Event;

    public final class GameplayDataEvent extends Event
    {
        public var data:Object;
        public function GameplayDataEvent(value:Object) { super("PEOData"); data = value; }
    }
}
