# flux_t — A Deterministic RAII-Driven Concurrency Topography for Modern C++

**flux_t** is a header-only C++20 concurrency and distributed-computing runtime that models execution as *Inertia & Flow* across a strictly RAII-defined physical topography.  
It abandons global schedulers, actor registries, and dynamic orchestration in favor of a mathematically clean, deterministic, type-safe model built from a minimal set of primitives:

`vessel_t`, `conduits_t`, `valve_t`, `nexus_t`, `hub_t`, `catalysts_t`, `reaction_t`, `pulse_t`, `ancestry_t`.

The result is a system where:

- flow replaces execution  
- topography replaces orchestration  
- RAII replaces lifecycle management  
- type signatures replace message routing  
- inertia replaces load balancing  
- and the C++ compiler becomes the only dependency injection framework you need

---

## Table of Contents

- [1. Introduction](#1-introduction)
- [2. Quick Start](#2-quick-start)
- [3. Core Concepts](#3-core-concepts)
  - [3.1 Inertia & Flow](#31-inertia--flow)
  - [3.2 Topography](#32-topography)
  - [3.3 Distributed Tissue](#33-distributed-tissue)
  - [3.4 Reaction Model](#34-reaction-model)
  - [3.5 Gravity Routing](#35-gravity-routing)
  - [3.6 Nested Stability](#36-nested-stability)
- [4. API Overview](#4-api-overview)
- [5. Usage Patterns](#5-usage-patterns)
- [6. Example](#6-example)
- [7. Design Principles](#7-design-principles)
- [8. Contributing](#8-contributing)
- [9. License](#9-license)

---

# 1. Introduction

`flux_t` is a concurrency runtime that treats computation as **flow** rather than execution.  
Instead of pushing tasks into a global thread pool or sending messages to actors, you define a *physical topography* of nested RAII objects:

```
vessel → conduits → valve → nexus → hub → catalysts → reactions
```

Pulses flow through this topography.  
Catalysts react to pulses.  
The system breathes in when the scope begins and breathes out when the scope ends.

There is no global state.  
There is no registry.  
There is no start/stop lifecycle.  
There is only **composition**.

---

# 2. Quick Start

## Install

Copy the `include/flux_t/` directory into your project or add it to your include path.

## Build example

```bash
mkdir build && cd build
cmake ..
cmake --build .
./hello_flux
```

## Minimal example

```cpp
#include <flux_t/flux_system.hpp>
#include <iostream>

struct my_pulse : flux_t::pulse_t<my_pulse>
{
    int value{};
};

struct printer_t : flux_t::reaction_t<printer_t, my_pulse>
{
    using base_t = flux_t::reaction_t<printer_t, my_pulse>;
    explicit printer_t(flux_t::catalysts_t& pool) noexcept : base_t(pool) {}

    void respond_t(const my_pulse& p)
    {
        std::cout << "
[printer]

 value = " << p.value << '\n';
    }
};

int main()
{
    using namespace flux_t;

    vessel_t vessel(2);
    conduits_t conduits(vessel);
    valve_t valve(conduits);
    valve.open_t();

    nexus_t::config_t cfg{"local", {}};
    nexus_t nexus(conduits, cfg);
    hub_t hub(nexus);
    catalysts_t pool(hub);

    printer_t printer(pool);

    nexus.broadcast_t(valve, my_pulse{42});
}
```

---

# 3. Core Concepts

## 3.1 Inertia & Flow

Traditional runtimes treat computation as a sequence of instructions executed by a scheduler.  
`flux_t` replaces this with:

- **pulses**: immutable, typed packets of information  
- **flow**: movement of pulses through conduits  
- **inertia**: the instantaneous load of each conduit  
- **gravity routing**: pulses “fall” into the conduit with the lowest inertia  

This yields deterministic routing without locks, registries, or global schedulers.

---

## 3.2 Topography

The system is defined by nested RAII objects:

- **vessel_t** — defines total capacity  
- **conduits_t** — worker channels with FIFO queues  
- **valve_t** — controls flow (open/close/drain)  

This is the physical space in which pulses move.

---

## 3.3 Distributed Tissue

Above the topography lies the routing and reaction layer:

- **nexus_t** — routing gateway  
- **hub_t** — domain boundary  
- **catalysts_t** — reaction domain  

Catalysts register and deregister automatically via RAII.

---

## 3.4 Reaction Model

- **pulse_t** — typed signal  
- **reaction_t** — CRTP base for catalysts  
- **ancestry_t** — compile-time dependency injection  

A reaction declares exactly which pulse types it handles:

```cpp
reaction_t<MyReaction, PulseA, PulseB>
```

No dynamic casting.  
No message IDs.  
No runtime registration.

---

## 3.5 Gravity Routing

When a pulse is broadcast:

1. **The nexus inspects the instantaneous inertia** of every conduit.  
2. **The conduit with the lowest inertia is selected.**  
3. **A task is injected into the selected conduit**, capturing the pulse by value.  
4. **All catalysts in the domain react to the pulse**, in deterministic order.

Gravity Routing is:

- O(N) over the number of conduits  
- lock-free for routing decisions  
- deterministic given identical state  
- fully RAII-driven  

The topography *is* the scheduler.

---

## 3.6 Nested Stability

Lifecycle is purely RAII:

1. Construct vessel → conduits → valve → nexus → hub → catalysts → reactions  
2. Open valve  
3. Inject pulses  
4. Close valve  
5. Drain  
6. Automatic teardown in reverse order  

No `start()` .  
No `stop()`.  
No global shutdown logic.

---

# 4. API Overview

## Topography

| Type         | Responsibility                       |
|--------------|--------------------------------------|
| `vessel_t`  | Defines capacity (number of conduits) |
| `conduits_t` | Worker threads + FIFO queues         |
| `valve_t`   | Flow control (open/close/drain)    |

## Routing & Domains

| Type          | Responsibility             |
|---------------|-----------------------------|
| `nexus_t`   | Gravity routing engine      |
| `hub_t`     | Domain boundary            |
| `catalysts_t` | Reaction domain registry    |

## Reaction Model

| Type                       | Responsibility                          |
|----------------------------|-----------------------------------------|
| `pulse_t<T>`               | Typed signal                            |
| `reaction_t<Derived, Pulses...>` | CRTP reaction base                   |
| `ancestry_t<...>`           | Compile-time dependency injection       |

---

# 5. Usage Patterns

## Single-header usage

```cpp
#include <flux_t/flux_system.hpp>
```

## Strict RAII

All lifecycle is implicit.  
Construction defines the system.  
Destruction tears it down safely.

## Type-safe reactions

```cpp
struct my_pulse : flux_t::pulse_t<my_pulse> {};

struct printer_t : flux_t::reaction_t<printer_t, my_pulse>
{
    using base_t = flux_t::reaction_t<printer_t, my_pulse>;
    explicit printer_t(flux_t::catalysts_t& pool) noexcept : base_t(pool) {}

    void respond_t(const my_pulse& p) { /* ... */ }
};
```

## Deterministic routing

```cpp
nexus.broadcast_t(valve, my_pulse{42});
```

---

# 6. Example

A complete example is provided in:

```
examples/hello_flux.cpp
```

It demonstrates:

- constructing the topography  
- opening the valve  
- broadcasting pulses  
- reacting to pulses  
- draining and shutting down safely  

---

# 7. Design Principles

`flux_t` adheres to the following strict constraints:

- Header-only  
- Pure RAII  
- No global state  
- No macros  
- No dynamic registration  
- No start/stop lifecycle  
- Deterministic routing  
- Type-safe reactions  
- 4-space indentation  
- ASCII-only  
- C++20-only  

These constraints ensure the system remains:

- predictable  
- maintainable  
- portable  
- testable  
- safe  

---

# 8. Contributing

Contributions must follow the architectural constraints:

- No `.cpp` files  
- No macros  
- No global state  
- No dynamic registration  
- No runtime DI  
- All new public APIs must include Doxygen comments  
- Formatting must follow the 4-space indentation rule  

---

# 9. License

MIT License — see `LICENSE`.
