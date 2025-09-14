# regbus — tiny, header-only real-time “register bus” for C++17

**regbus** is a **zero-allocation**, **header-only** C++17 library for sharing data between producers and consumers in real-time systems (embedded, robotics, telemetry, games). It provides **double-buffered “latest value” registers** and **edge-trigger commands** with **lock-free-ish** access semantics and **coherent snapshot reads**—perfect for sensor → fusion → UI pipelines.

> TL;DR: If you’ve ever wanted a fast, simple, threadsafe way to say “here’s the **latest** IMU sample” or “a **command** was issued”, without queues or mallocs, this is it.

---

## Inspiration

Typical message/queue approaches introduce latency, backpressure, and allocations. In many realtime pipelines you don’t want a backlog—you want the **freshest value** right now. **regbus** gives you:

- **Overwrite-latest** semantics (no queue churn)
- **Coherent reads** (no torn structs) via double buffering + atomics
- **No dynamic allocation** (great for MCUs)
- **Header-only, portable**: works on Ubuntu/Windows/macOS; works on microcontrollers (ESP-IDF, etc.)
- **Strong typing at compile time**: your own key enum + type mapping
- **Minimal API** that’s easy to test on host (gtest) and reuse across projects

Inspired by the *Modbus* mental model (input/holding registers & coils), but modern C++ and zero-overhead.

---

## Features

- **`DBReg<T>`** — double-buffered register: writers store a value; readers get a **coherent snapshot** of the latest value.
- **`CmdReg<T>`** — command/coil: `post()` once, `consume()` once (edge-trigger).
- **`Registry<Key, Traits, Keys...>`** — compile-time registry over your **enum class** of keys and a **Traits** map (`type` + `kind`). No RTTI, no maps, no heap.
- **Header-only**. No dependencies beyond the STL `<atomic>`, `<tuple>`, `<type_traits>`.
- **Lock-free-ish**: readers/writers never block each other; small atomics with `release/acquire` semantics.
- **Deterministic footprint**: `sizeof(Registry)` known at compile-time.

> Constraint: `T` for `DBReg<T>` must be **trivially copyable** (POD). This keeps copies fast and avoids hidden allocations.

---

## Quick start

### 1) Add to your project (as a submodule or vendor the headers)

```bash
git submodule add https://github.com/Mrunmoy/regbus.git third_party/regbus
```

CMake (consumer project):

```cmake
# Add regbus headers to your include paths
target_include_directories(your_target PRIVATE ${CMAKE_SOURCE_DIR}/third_party/regbus/include)
target_compile_features(your_target PRIVATE cxx_std_17)
```

### 2) Define your key set and traits

```cpp
#include "regbus/Registry.hpp"

// Your keys
enum class Key : uint8_t { IMU_RAW, FUSION_STATE, CMD_RESET };

// Your trivially copyable types
struct IMURaw { uint64_t t_us; float ax, ay, az, gx, gy, gz; };
struct FusionState { uint64_t t_us; float qw, qx, qy, qz; };

// Map keys -> (type, kind)
template<Key K> struct Traits;
template<> struct Traits<Key::IMU_RAW>      { using type = IMURaw;      static constexpr regbus::Kind kind = regbus::Kind::Data; };
template<> struct Traits<Key::FUSION_STATE> { using type = FusionState; static constexpr regbus::Kind kind = regbus::Kind::Data; };
template<> struct Traits<Key::CMD_RESET>    { using type = bool;        static constexpr regbus::Kind kind = regbus::Kind::Cmd;  };

// Build your Registry over a fixed key list (compile-time)
using Reg = regbus::Registry<Key, Traits, Key::IMU_RAW, Key::FUSION_STATE, Key::CMD_RESET>;
```

### 3) Use it

```cpp
Reg reg;

// Producer (e.g., IMU task)
IMURaw s{123456, 0.1f, 0.0f, 9.8f, 0.01f, 0.02f, 0.03f};
reg.write<Key::IMU_RAW>(s);

// Consumer (e.g., Fusion task)
IMURaw r{}; uint32_t seq=0;
if (reg.read<Key::IMU_RAW>(r, &seq)) {
  // r is a coherent snapshot of the latest sample
}

// Command (edge-trigger)
reg.post<Key::CMD_RESET>(true);
bool reset=false;
if (reg.consume<Key::CMD_RESET>(reset)) {
  // handled once
}
```

---

## Concepts & mental model

- **Data registers** are like Modbus “input/holding registers”: overwrite-latest values that producers update and consumers read at will.
- **Command registers** are like “coils”: they carry an event/command that is **consumed once** by a handler.
- **No queues**: if updates arrive faster than readers, readers still get the most recent state (no backlog).
- **Coherent snapshot**: a read never returns a half-updated struct—thanks to the double buffer and sequence check.

### Threading model

- Many readers, any number of writers (per key). If multiple writers hit the same key, **last writer wins**.
- Writers perform: copy into inactive slot → publish by flipping an atomic index (`release`).
- Readers perform: read active index + seq → copy → validate seq is unchanged (`acquire`) → return or retry.

---

## Headers overview

- `include/regbus/DBReg.hpp`  
  Double-buffered latest-value register (`DBReg<T>`). Requires `T` to be trivially copyable.

- `include/regbus/CmdReg.hpp`  
  Command/coil register (`CmdReg<T>`). `post()` sets a pending value; `consume(out)` reads **once** and clears.

- `include/regbus/Registry.hpp`  
  Generic compile-time registry:
  ```cpp
  template <typename Key, template<Key> class Traits, Key... Keys>
  class Registry { ... };
  ```
  You provide an **enum class** `Key` and a **Traits** template that maps each `Key` to:
  ```cpp
  template<Key K> struct TraitsK {
    using type = ...;                 // trivially copyable type
    static constexpr regbus::Kind kind = regbus::Kind::Data or Kind::Cmd;
  };
  ```
  Then build a `Registry` over a fixed key list at compile time.

---

## Examples

Build the example program (enabled by default in CMake with `REGBUS_BUILD_EXAMPLES=ON`).

```bash
mkdir -p build && cd build
cmake -DREGBUS_BUILD_TESTS=ON -DREGBUS_BUILD_EXAMPLES=ON ..
cmake --build . -j
./example_minimal
```

**`examples/minimal_registry.cpp`** demonstrates a writer thread posting IMU samples while the main thread reads them and consumes a `CMD_RESET` coil.

---

## Unit tests

We ship gtests for the core pieces (enabled via `-DREGBUS_BUILD_TESTS=ON`).

```bash
mkdir -p build && cd build
cmake -DREGBUS_BUILD_TESTS=ON ..
cmake --build . -j
ctest --output-on-failure
```

Tests include:
- `DBReg` latest-wins & coherence under write flips
- `CmdReg` edge-trigger semantics
- `Registry` round-trip reads/writes and command consumption
- A **size budget** test to catch accidental bloat (`static_assert(bytes() <= N)`).


---

## Performance notes

- **Latency**: a write is a single struct copy + two atomics. A read is a single struct copy + two atomics with a rare retry if the writer flips during the read.
- **Throughput**: suitable for hundreds to thousands of Hz on typical desktops; on MCUs (e.g., ESP32-S3) hundreds of Hz per key is common, provided your types are small.
- **Sizes**: Keep register types compact (avoid large arrays); prefer `float`/`int` fields.
- **False sharing**: each `DBReg<T>` keeps both buffers in one object; alignment (`alignas(16)`) reduces cache issues.

---

## Design constraints (intentional)

- **No dynamic allocations** inside regbus
- **No RTTI / exceptions** required (you may compile your project with `-fno-rtti -fno-exceptions`)
- **Trivially copyable** types only for data registers (`static_assert` enforces this)
- **No built-in observers** (yet). If you need notifications, either poll at your cadence or build a tiny observer that wakes readers on a timer / condition variable. (A fixed-capacity observer utility may land later as an optional header.)

---

## Using with embedded (ESP-IDF/FreeRTOS)

- regbus itself does **not** include FreeRTOS; it’s pure C++17.  
- Inject a `Registry` instance into your tasks by reference; producers call `reg.write<...>`; consumers call `reg.read<...>` on their cadence.  
- No heap usage, so it’s safe inside ISR-free task code paths.

Example task split (pseudo):
```cpp
// Producer task
void ImuTask(Reg& reg, IMU& imu) {
  for (;;) {
    IMURaw s = imu.sample();
    reg.write<Key::IMU_RAW>(s);
    vTaskDelayUntil(...);
  }
}

// Consumer task
void FusionTask(Reg& reg, Filter& f) {
  IMURaw s{};
  if (reg.read<Key::IMU_RAW>(s)) {
    auto fs = f.step(s);
    reg.write<Key::FUSION_STATE>(fs);
  }
}
```

---

## Troubleshooting / FAQ

**Q: Why not a queue/channel?**  
A: Queues preserve *all* messages; that causes backpressure and latency. For control/telemetry, you usually want **the latest** state—this is exactly what `DBReg` does.

**Q: What if my type isn’t trivially copyable?**  
A: Adapt it: store only POD fields (IDs, indices, fixed-size arrays). Keep ownership of dynamic memory elsewhere.

**Q: Multiple writers to the same key?**  
A: Supported, but last writer wins. If you need arbitration, split keys or add a version/priority field to your type.

**Q: Is it real “lock-free”?**  
A: It uses atomics and never blocks; we call it “lock-free-ish”. Progress is guaranteed for readers/writers under the usual memory model. It’s not using platform CAS loops beyond the atomic index/sequence flips.

**Q: Can I get history / plotting buffers?**  
A: Not yet in core. You can layer a `RingReg<T,N>` (fixed array + head index) beside a `DBReg<T>`—we may add this as an optional header.

---


## Roadmap

- Optional **RingReg<T,N>** for short history windows
- Optional **observer/notify** helper (fixed maximum subscribers; zero heap)
- Benchmark utilities and more examples (embedded + desktop)

---

## License

MIT — see [LICENSE](LICENSE).

---

## Acknowledgements

Built to support high-rate sensor fusion pipelines without queues or mallocs. Inspired by the Modbus “register” mental model and the reality that **latest state** beats **old messages** in control systems.

---

Happy hacking! If you use regbus in your project, consider sharing a link in an issue so others can learn from your setup.