# Model Architecture

This directory implements a **policy-based data synchronization system** for ESP32 with real-time WebSocket updates and persistent preferences storage.

## Core Components

### 1. **ModelVar.h** — Policy-based field wrapper

Defines the `Var<T, WsMode, PrefsMode, WriteMode>` template which wraps any type with fine-grained control over:

- **WsMode**: How to emit the field to WebSocket clients
  - `Value` — send actual data as JSON
  - `Meta` — send only metadata like `{"type":"secret", "initialized":true}` (Variant A pattern for passwords)
  - `None` — omit from WebSocket entirely

- **PrefsMode**: Whether to persist to device preferences
  - `On` — write to preferences on every change
  - `Off` — transient only (memory only)

- **WriteMode**: Whether remote clients can update this field
  - `On` — accept incoming updates (writable)
  - `Off` — read-only (server-only)

#### Convenience aliases:
```cpp
VarWsPrefsRw     // = Var<T, Value, On,  On>   — most common
VarWsRo          // = Var<T, Value, Off, Off>  — display-only
VarMetaPrefsRw   // = Var<T, Meta,  On,  On>   — secure passwords
```

All 8 combinations available for custom scenarios.

### 2. **ModelSerializer.h** — JSON dispatch logic

Routes serialization through **three levels of abstraction**:

1. **Custom TypeAdapters** (for List, Button, PointRingBuffer)
   - Complex types that need custom JSON format
   - Defined in `types/`

2. **c_str() fallback** (for StringBuffer, String)
   - Types with string interface
   - Falls back to literal string in JSON

3. **Direct assignment** (for int, float, bool)
   - Plain scalars
   - Direct JSON native types

Uses **SFINAE compile-time introspection** to avoid overhead and ensure type safety.

### 3. **ModelBase.h** — Topic management & WebSocket integration

Core framework that:
- Registers topics (WifiSettings, AdminSettings, etc.)
- Broadcasts changes via WebSocket
- Listens for incoming updates
- Persists to preferences
- Manages envelopes (`{"meta":{...}, "data":{...}}`)

Static JSON documents moved to **BSS allocation** (not stack) to avoid stack exhaustion in tasks.

### 4. **src/types/** — Domain-specific adapters

Each file provides `TypeAdapter<T>` for custom types:

- `ModelTypeButton.h` — Button triggers with action confirmation
- `ModelTypePrimitive.h` — Basic types (int, float, bool)
- `ModelTypeList.h` — List<T, N> fixed-size arrays with JSON arrays
- `ModelTypePointRingBuffer.h` — Ring buffers for time-series graphs
- `ModelTypeTraits.h` — Base TypeAdapter template and detection helpers

## Data Flow

### Broadcasting (Server → Client):
```
Model field updated
    ↓
Var<T>::set() → notify callback
    ↓
ModelBase::broadcastTopic()
    ↓
writeOne() — applies WsMode policy
    ↓
TypeAdapter or direct write → JSON
    ↓
WebSocket envelope → client
```

### Persisting (Server → NVS):
```
Var<T>::set() → notify callback
    ↓
ModelBase::saveTopic()
    ↓
writeOnePrefs() — ignores WsMode, always full value
    ↓
TypeAdapter or c_str → JSON
    ↓
Preferences::putString() → NVS
```

### Receiving (Client → Server):
```
WebSocket message received
    ↓
ModelBase::handleIncoming()
    ↓
readOne() — checks WriteMode (reject if Off)
    ↓
TypeAdapter or direct read ← JSON
    ↓
Var<T>::set() with new value
    ↓
Callback fires → app logic
```

## Memory Optimization

All static JSON documents allocated in **BSS segment** (not stack):

```cpp
// In ModelBase.h
static StaticJsonDocument<2048> gEnvelopeDoc;
static StaticJsonDocument<2048> gDataOnlyDoc;
static StaticJsonDocument<2048> gPrefsDoc;
static StaticJsonDocument<2048> gWsDoc;

// Before each use:
doc.clear();  // Reset state to prevent cross-test pollution
```

**Capacity rationale**: 2048 bytes safely holds:
- Full WiFi settings (SSID, password, 20 networks)
- Admin log ring buffer (5 events)
- Button metadata

## Trait Detection (SFINAE)

The system uses C++11 template metaprogramming to detect capabilities:

```cpp
has_c_str<T>                    // Has c_str() method?
has_set_cstr<T>                 // Has set(const char*) method?
has_typeadapter_read<T>         // Has TypeAdapter<T>::read?
has_typeadapter_write_ws<T>     // Has TypeAdapter<T>::write_ws?
has_typeadapter_write_prefs<T>  // Has TypeAdapter<T>::write_prefs?
is_string_like<T>               // Both c_str + set?
```

These compile-time checks avoid runtime branching and allow specialization without `if constexpr`.

## Adding a New Field

1. **Choose policy**:
   ```cpp
   fj::VarWsPrefsRw<MyType> my_field;  // value, persisted, writable
   ```

2. **Add to struct schema**:
   ```cpp
   typedef fj::Schema<MySettings,
                      fj::Field<MySettings, decltype(my_field)>>
       SchemaType;
   ```

3. **Register in ModelBase**:
   ```cpp
   registerTopic("my_settings", my_settings);
   ```

4. **If complex type**, create `ModelTypeMyType.h`:
   ```cpp
   struct TypeAdapter<MyType> {
     static void write_ws(const MyType& obj, JsonObject out) { ... }
     static void write_prefs(const MyType& obj, JsonObject out) { ... }
     static bool read(MyType& obj, JsonObject in, bool strict) { ... }
   };
   ```

## Logging

All operations emit `LOG_TRACE` (only at TRACE level) showing:
- Field name and access path
- Type dispatch decision (TypeAdapter vs. c_str vs. direct)
- Parse results and errors

Reduces noise at INFO/DEBUG levels while enabling deep inspection during development.

## C++11 Compatibility

- No `if constexpr` (C++17)
- No fold expressions (C++17)
- SFINAE only, template specialization
- Works with ArduinoJson 6.x ABI
