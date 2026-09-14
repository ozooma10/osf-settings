"""Adapt specific argument-less vanilla event listeners to Flash's strict arity.

Only generated preview copies use this. Each listed listener has no local
variables: adding an unused ...rest register preserves its original bytecode.
"""

CALLBACKS = {
    ("ButtonBar", "RefreshButtons"),
    ("ButtonBar", "RefreshAfterRedraw"),
    ("SettingsOptionList", "onBeginDrag"),
    ("SettingsOptionList", "onEndDrag"),
    ("SettingsOptionListEntry", "onCheckboxValueChange"),
}


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def byte(self):
        result = self.data[self.pos]
        self.pos += 1
        return result

    def uint(self):
        result = 0
        for shift in range(0, 35, 7):
            byte = self.byte()
            result |= (byte & 127) << shift
            if byte < 128:
                return result
        raise ValueError("Invalid ABC integer")

    def skip_uints(self, count):
        for _ in range(count):
            self.uint()


def adapt_callbacks(data):
    data = bytearray(data)
    reader = Reader(data)
    reader.pos = 4  # ABC minor/major version
    for _ in range(2):
        reader.skip_uints(max(0, reader.uint() - 1))
    double_count = reader.uint()
    reader.pos += max(0, double_count - 1) * 8
    strings = [""]
    for _ in range(1, reader.uint()):
        size = reader.uint()
        strings.append(bytes(data[reader.pos:reader.pos + size]).decode("utf-8"))
        reader.pos += size
    for _ in range(1, reader.uint()):
        reader.byte(); reader.uint()
    for _ in range(1, reader.uint()):
        reader.skip_uints(reader.uint())
    names = [""]
    for _ in range(1, reader.uint()):
        kind = reader.byte()
        name = ""
        if kind in (7, 13):
            reader.uint(); name = strings[reader.uint()]
        elif kind in (15, 16):
            name = strings[reader.uint()]
        elif kind in (9, 14):
            name = strings[reader.uint()]; reader.uint()
        elif kind in (27, 28):
            reader.uint()
        elif kind == 29:
            reader.uint(); reader.skip_uints(reader.uint())
        elif kind not in (17, 18):
            raise ValueError(f"Unsupported ABC multiname: {kind}")
        names.append(name)

    methods = []
    for _ in range(reader.uint()):
        count = reader.uint()
        reader.skip_uints(count + 2)  # return type, argument types, debug name
        flag_pos = reader.pos
        flags = reader.byte()
        methods.append((count, flag_pos, flags))
        if flags & 8:
            for _ in range(reader.uint()):
                reader.uint(); reader.byte()
        if flags & 128:
            reader.skip_uints(count)
    for _ in range(reader.uint()):
        reader.uint(); reader.skip_uints(reader.uint() * 2)

    selected = {}

    def traits(owner=""):
        for _ in range(reader.uint()):
            name = names[reader.uint()]
            tag = reader.byte()
            kind = tag & 15
            if kind in (0, 6):
                reader.skip_uints(2)
                if reader.uint():
                    reader.byte()
            elif kind in (1, 2, 3):
                reader.uint()
                method = reader.uint()
                if (owner, name) in CALLBACKS:
                    selected[method] = owner + "." + name
            elif kind in (4, 5):
                reader.skip_uints(2)
            else:
                raise ValueError(f"Unsupported ABC trait: {kind}")
            if tag & 64:
                reader.skip_uints(reader.uint())

    class_count = reader.uint()
    for _ in range(class_count):
        owner = names[reader.uint()]
        reader.uint()
        if reader.byte() & 8:
            reader.uint()
        reader.skip_uints(reader.uint())
        reader.uint(); traits(owner)
    for _ in range(class_count):
        reader.uint(); traits()
    for _ in range(reader.uint()):
        reader.uint(); traits()
    patched = set()
    for _ in range(reader.uint()):
        method = reader.uint()
        reader.uint()  # max stack
        local_pos = reader.pos
        local_count = reader.uint()
        if method in selected:
            count, flag_pos, flags = methods[method]
            if count != 0 or local_count != 1 or flags & (1 | 4):
                raise ValueError(f"Vanilla callback changed; review adapter: {selected[method]}")
            data[flag_pos] |= 4  # NEED_REST; incoming Event is ignored
            data[local_pos] = 2  # this + unused rest array (single-byte U30)
            patched.add(method)
        reader.skip_uints(2)  # initial/max scope
        size = reader.uint(); reader.pos += size
        reader.skip_uints(reader.uint() * 5)  # exceptions
        traits()
    if reader.pos != len(data) or patched != selected.keys():
        raise ValueError("Incomplete ABC callback adaptation")
    return bytes(data)
