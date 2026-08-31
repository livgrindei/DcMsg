# DcMsg

A small, self-describing binary serialization format for C++17, with a
binary-compatible Python port. Think a minimal, hand-rolled alternative to
protobuf/msgpack for cases where you want a compact wire format without a
schema compiler or an external dependency.

## Features

- Compact binary wire format: 3x `uint64` header (version, total size,
  element count) followed by named, typed elements.
- Typed accessors for every scalar type (`bool`, signed/unsigned 8/16/32/64-bit
  integers, `float`, `double`), plus `string`, nested messages, raw byte
  buffers, fixed-element arrays (one `Add*Array`/`Get*Array` pair per scalar
  type), string arrays, and message arrays.
- `Update*` methods overwrite an existing scalar element in place with no
  buffer resize; `Delete(name)` removes any element by name (structural
  shift + re-index of everything after it).
- `GetLastError()` returns a `DcMsg::EError` describing why the last call on
  an instance failed, alongside human-readable logging.
- A read-only mode: construct a `DcMsg` directly over an existing buffer
  (e.g. one just received off the wire) without copying it.
- A Python port (`python/dcmsg.py`, using `struct`/`numpy`) that is
  wire-compatible with the C++ implementation — encode in one language,
  decode in the other.

## Wire format

A `DcMsg` message is a bytes array with a header and a succession of
`DcMsg` elements:

```
   HEADER   |    DcMsg     | DcMsg ...
 3 uint64   | min 18 bytes | min 18 bytes
```

**Header** — 3x `uint64`:

```
 VERSION |                   SIZE                 |        ELEMENTS
 uint64  |                  uint64                |         uint64
  1000   | total size in bytes (including header) | no of DcMsg elements
```

**Scalar element** (typecode `BOOL` to `DOUBLE`):

```
  TYPECODE     |          NAME            |  VALUE
   1 byte      |       16 bytes           | 1 byte (BOOL, UBYTE, BYTE) or 4 bytes (UINT, INT, FLOAT) or 8 bytes (ULONG, LONG, DOUBLE)
TypeCode enum  | max 15 char + null term  |  value
```

**Variable-length element** (typecode `STRING`, `MESSAGE` or `MEMORYARRAY`):

```
  TYPECODE     |          NAME            |      SIZE     |    VALUE
   1 byte      |       16 bytes           |    4 bytes    |  SIZE bytes
TypeCode enum  | max 15 char + null term  | size (uint32) |    bytes
```
String values are not null-terminated.

**Fixed-element array** (typecode `BOOLARRAY` .. `DOUBLEARRAY`) — VALUE is
`COUNT * element size` raw bytes:

```
  TYPECODE     |          NAME            |      SIZE     |    VALUE
   1 byte      |       16 bytes           |    4 bytes    |  SIZE bytes
TypeCode enum  | max 15 char + null term  | size (uint32) |    packed elements
```

**Object array element** (typecode `STRINGARRAY` or `MESSAGEARRAY`) — each
item is length-prefixed:

```
  TYPECODE     |          NAME            |  TOTAL SIZE   |    NO ITEMS   |  ITEM1 SIZE   | ITEM1 PAYLOAD
   1 byte      |       16 bytes           |    4 bytes    |    4 bytes    |     4 bytes   |
TypeCode enum  | max 15 char + null term  | size (uint32) | size (uint32) | size (uint32) |    bytes
```
String values are not null-terminated.

Element names are looked up linearly and are limited to 15 characters (plus a
null terminator) — this trades O(n) lookups and a small per-element overhead
for a dead-simple, allocation-light format.

## Building

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /your/install/prefix   # optional
```

This produces a static library (`libdcmsg.a`) by default; pass
`-DBUILD_SHARED_LIBS=ON` to build a shared library instead.

### Using it from another CMake project

After installing, or via `add_subdirectory`:

```cmake
find_package(dcmsg REQUIRED)   # if installed
# or: add_subdirectory(path/to/dcmsg-lib)

target_link_libraries(your_target PRIVATE dcmsg::dcmsg)
```

## C++ usage

```cpp
#include <dcmsg/dcmsg.h>

DS::DcMsg msg;
msg.AddUInt("Channel", 3);
msg.AddDouble("Value", 101.15);
msg.AddString("Label", "sensor-3");

uint64_t size = 0;
void* buffer = msg.GetData(size);   // ready to send over the wire

// ... on the receiving side, constructed directly over the received buffer:
DS::DcMsg received(buffer, size);
if (!received.IsValid())
{
    // received.GetLastError() describes why
}

uint32_t channel;
double value;
received.GetUInt("Channel", channel);
received.GetDouble("Value", value);
```

Nesting works the same way, via `AddMessage`/`GetMessage` for a single nested
`DcMsg`, or `AddMessageArray`/`GetMessageArray` for `std::vector<DcMsg>`.

`received` above is read-only — it was constructed directly over an external
buffer, so `Add*`/`Update*`/`Delete` all reject on it. To get an independent,
mutable copy of its content, use `Clone()`:

```cpp
DS::DcMsg editable = received.Clone();
editable.UpdateUInt("Channel", 4);   // works; received is untouched
```

`Clone()` deep-copies the underlying buffer regardless of whether the source
is read-only or already editable, so it also works as a plain "snapshot
before mutating further" on a writable instance.

## Python usage

```python
from dcmsg import DcMsg

msg = DcMsg()
msg.AddUInt("Channel", 3)
msg.AddDouble("Value", 101.15)
msg.AddString("Label", "sensor-3")

data = msg.GetData()          # bytes, wire-compatible with the C++ side

received = DcMsg(data)       # parses in the constructor
values = received.GetDictionary()
print(values["Channel"], values["Value"], values["Label"])
```

Any change to the wire format on either side (`src/dcmsg.cpp`/
`include/dcmsg/dcmsg.h` or `python/dcmsg.py`) must be mirrored in the
other implementation, and a breaking format change should bump
`MESS_VERSION`/`MIN_MESS_VERSION` in `dcmsg.h`.

## Examples

Built automatically alongside the library (`DCMSG_BUILD_EXAMPLES`, ON by
default when building this repo standalone):

- [`examples/quickstart.cpp`](examples/quickstart.cpp) /
  [`examples/quickstart.py`](examples/quickstart.py) — scalars, a nested
  message, a fixed array, and error handling, in both languages.
- [`examples/pubsub_demo.cpp`](examples/pubsub_demo.cpp) — recreates the
  shape of the FZMQ-based publisher/subscriber demo this library used to
  ship with, minus the transport dependency: a "publisher" thread builds one
  `DcMsg` per tick and a "subscriber" thread decodes each one as it
  arrives, connected by a small in-process byte-buffer channel instead of a
  real ZMQ PUB/SUB socket pair. Swap the channel for your transport of
  choice — the `DcMsg` encode/decode calls on either side don't change.

```sh
cmake --build build --target quickstart pubsub_demo
./build/examples/quickstart
./build/examples/pubsub_demo
python3 examples/quickstart.py
```

## License

MIT — see [LICENSE](LICENSE).
