# regbus — tiny, header-only real-time “register bus” for C++17

[![CI](https://github.com/Mrunmoy/regbus/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Mrunmoy/regbus/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/Mrunmoy/regbus?display_name=tag&sort=semver)](https://github.com/Mrunmoy/regbus/releases)

**regbus** is a **zero-allocation**, **header-only** C++17 library for sharing data between producers and consumers in real-time systems (embedded, robotics, telemetry, games). It provides **double-buffered “latest value” registers** and **edge-trigger commands** with **lock-free-ish** access semantics and **coherent snapshot reads**—ideal for sensor → fusion → UI pipelines.

> TL;DR: If you want a fast, simple, threadsafe way to say “here’s the **latest** sample” or “a **command** was issued”, without queues or mallocs, this is it.

---

## Why regbus?

Typical message/queue approaches introduce latency, backpressure, and allocations. In many real‑time pipelines you don’t want a backlog—you want the **freshest value** right now. **regbus** gives you:

- **Overwrite-latest** semantics (no queue churn)
- **Coherent reads** via double buffering + atomics (no torn structs)
- **No dynamic allocation** (great for MCUs)
- **Header‑only, portable**: Linux/macOS/Windows + microcontrollers (ESP‑IDF, etc.)
- **Strong typing at compile time**: your own key enum + trait map
- **Minimal API**, easy to unit-test on host (gtest) and reuse across projects

Inspired by the *Modbus* mental model (holding/input registers & coils), but modern C++ and zero overhead.

---

## Features

- **`DBReg<T>`** — double-buffered register: writers store a value; readers get a **coherent snapshot** of the latest value.
- **`CmdReg<T>`** — command/coil: `post()` once, `consume()` once (edge-trigger).
- **`Registry<Key, Traits, Keys...>`** — compile-time registry over your **enum class** keys with a Traits map (`type` + `kind`). No RTTI, no maps, no heap.
- **Header-only**. Depends only on `<atomic>`, `<tuple>`, `<type_traits>`.
- **Lock-free-ish**: writers/readers never block each other; memory_order tuned (`release/acquire`).
- **Deterministic footprint**: `sizeof(Registry)` is known at compile time.

> Constraint: `T` for `DBReg<T>` must be **trivially copyable** (POD).

---

## Quick start

### Option A) Use as a submodule (firmware-friendly)

```bash
git submodule add https://github.com/Mrunmoy/regbus.git third_party/regbus
```

**CMake (consumer project):**
```cmake
# Bring in the target `regbus::regbus`
add_subdirectory(third_party/regbus)

# Link it (header-only interface target)
target_link_libraries(your_target PRIVATE regbus::regbus)
```

---

### Option B) Install & use with `find_package`

Install once (system- or user-prefix):
```bash
cmake -S regbus -B regbus/build -DCMAKE_BUILD_TYPE=Release
cmake --build regbus/build -j
cmake --install regbus/build --prefix ~/.local
```

**CMake (consumer project):**
```cmake
# If installed to a non-default prefix, set CMAKE_PREFIX_PATH when configuring:
# cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/.local

find_package(regbus 0.1.0 CONFIG REQUIRED)  # or newer
target_link_libraries(your_target PRIVATE regbus::regbus)
```

---

## Define your keys/traits and use

```cpp
#include "regbus/Registry.hpp"

// 1) Keys
enum class Key : uint8_t { IMU_RAW, FUSION_STATE, CMD_RESET };

// 2) Types (must be trivially copyable)
struct IMURaw      { uint64_t t_us; float ax, ay, az, gx, gy, gz; };
struct FusionState { uint64_t t_us; float qw, qx, qy, qz; };

// 3) Traits map
template<Key K> struct Traits;
template<> struct Traits<Key::IMU_RAW>      { using type = IMURaw;      static constexpr regbus::Kind kind = regbus::Kind::Data; };
template<> struct Traits<Key::FUSION_STATE> { using type = FusionState; static constexpr regbus::Kind kind = regbus::Kind::Data; };
template<> struct Traits<Key::CMD_RESET>    { using type = bool;        static constexpr regbus::Kind kind = regbus::Kind::Cmd;  };

// 4) Build a Registry over a fixed key list (compile-time)
using Reg = regbus::Registry<Key, Traits, Key::IMU_RAW, Key::FUSION_STATE, Key::CMD_RESET>;

// 5) Use
Reg reg;

IMURaw s{123456, 0.1f, 0.0f, 9.8f, 0.01f, 0.02f, 0.03f};
reg.write<Key::IMU_RAW>(s);

IMURaw r{}; uint32_t seq=0;
if (reg.read<Key::IMU_RAW>(r, &seq)) {
  // r is a coherent snapshot of the latest sample
}

reg.post<Key::CMD_RESET>(true);
bool reset=false;
if (reg.consume<Key::CMD_RESET>(reset)) {
  // handled once
}
```

---

## Versioning

This project uses **Semantic Versioning**.

- **0.y.z**: pre-1.0; API can evolve at **minor** bumps; fixes at **patch**.
- **1.0.0+**: stable API; breaking changes bump **major**.

A compiled-in version header is provided:

```cpp
#include "regbus/version.hpp"

static_assert(REGBUS_VERSION_AT_LEAST(0,1,0),
              "Need regbus >= 0.1.0");

printf("regbus %s (0x%06x)\n",
       regbus::version_string, regbus::version_hex);
```

When cutting a release, bump `include/regbus/version.hpp` and the `project(... VERSION X.Y.Z)` in the top-level CMakeLists, then tag (e.g., `v0.1.0`).

---

## Concepts & mental model

- **Data registers** (like Modbus input/holding registers): overwrite‑latest values; readers pull at their cadence.
- **Command registers** (like Modbus coils): one‑shot events—`post()` then `consume()` clears them.
- **No queues**: you always read the freshest state; no backlog or extra latency.
- **Coherent snapshot**: a read never returns a half-updated struct—thanks to double buffering + seq validation.

### Threading model

- Many readers, any number of writers per key (last writer wins).
- Writers: copy into inactive slot → publish by flipping an atomic index (`release`).
- Readers: read active index + seq → copy → recheck index/seq (`acquire`) → return or retry.

---

## Headers

- `include/regbus/DBReg.hpp` — double-buffered latest-value register (`DBReg<T>`). Enforces `T` is trivially copyable.
- `include/regbus/CmdReg.hpp` — command/coil (`CmdReg<T>`). `post()` sets pending; `consume(out)` reads once and clears.
- `include/regbus/Registry.hpp` — generic, compile-time registry over your `Key` + `Traits` + key list.
- `include/regbus/version.hpp` — version macros and constexprs (`REGBUS_VERSION_*`, `version_string`, etc.).

---

## Examples

Build the example program (enable with `REGBUS_BUILD_EXAMPLES=ON`):

```bash
mkdir -p build && cd build
cmake -DREGBUS_BUILD_TESTS=ON -DREGBUS_BUILD_EXAMPLES=ON ..
cmake --build . -j
./example_minimal
```

`examples/minimal_registry.cpp` shows a writer thread posting IMU samples while the main thread reads them and consumes a `CMD_RESET` coil.

---

## Unit tests

Enable with `REGBUS_BUILD_TESTS=ON`:

```bash
mkdir -p build && cd build
cmake -DREGBUS_BUILD_TESTS=ON ..
cmake --build . -j
ctest --output-on-failure
```

Included tests:

- `DBReg` latest-wins & coherence under flips
- `CmdReg` edge-trigger semantics
- `Registry` round-trip + command consume
- **Coherence stress**: no-tear patterns, monotonic sequences, double-read stability
- Optional **size budget** assertion (`static_assert(bytes() <= N)`).

---

## Performance notes

- **Latency**: write = one POD copy + two atomics; read = one POD copy + two atomics (rare retry under contention).
- **Throughput**: thousands of Hz on desktop; hundreds+ Hz on MCUs if your structs are small.
- **Structure sizing**: prefer compact PODs (floats/ints); avoid large arrays.
- **False sharing**: `alignas(16)` in `DBReg<T>` mitigates cache-line issues.

---

## Embedded (ESP‑IDF/FreeRTOS) usage

regbus is pure C++17. Inject a `Registry` instance into tasks; producers `write<...>()`; consumers `read<...>()` at their cadence. No heap usage inside regbus.

```cpp
void ImuTask(Reg& reg, IMU& imu) {
  for (;;) {
    reg.write<Key::IMU_RAW>(imu.sample());
    vTaskDelayUntil(...);
  }
}

void FusionTask(Reg& reg, Filter& f) {
  IMURaw s{};
  if (reg.read<Key::IMU_RAW>(s)) {
    reg.write<Key::FUSION_STATE>(f.step(s));
  }
}
```

---

## FAQ

**Why not a queue/channel?**  
Queues preserve *all* messages → backpressure & latency. For control/telemetry, you want **the latest** state; `DBReg` gives exactly that.

**What if my type isn’t trivially copyable?**  
Store POD fields only; keep dynamic memory elsewhere.

**Multiple writers to the same key?**  
Supported (last writer wins). If you need arbitration, split keys or add priority/version fields.

**Is it “lock-free”?**  
We use atomics, not mutexes; readers/writers never block. It’s “lock-free‑ish” with progress guarantees under standard memory models.

**Can I get history/plots?**  
Layer a fixed-size ring buffer beside `DBReg<T>`; a `RingReg<T,N>` may be added later as an optional header.

---

## Roadmap

- Optional **RingReg<T,N>** for short history
- Optional **observer/notify** helper (zero heap, fixed subscribers)
- Benchmarks and more examples

---

## License

MIT — see [LICENSE](LICENSE).

---

## Acknowledgements

Built to support high‑rate sensor fusion pipelines without queues or mallocs. Inspired by the Modbus “register” mental model and the reality that **latest state** beats **old messages** in control systems.

---

Happy hacking! If you use regbus in your project, please share a link in an issue so others can learn from your setup.