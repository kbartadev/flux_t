# flux_t 🌊
A strictly scoped, C++20 RAII-driven concurrency and message-passing runtime.

flux_t is a lightweight, header-only C++20 library designed to simplify multithreaded application design. By modeling execution as a physical topography of RAII-managed objects, it replaces manual start/stop lifecycles and global message buses with a deterministic, type-safe "flow" of data.

### 1. Philosophy: Topography over Orchestration
Traditional concurrency is often "unanchored"—relying on global schedulers or manual orchestration. flux_t abandons this in favor of a clean, physical model:

* Flow replaces execution: Computation is treated as the movement of data (Pulses) through a system.
* Topography replaces orchestration: The physical arrangement of your objects determines the system's behavior.
* RAII is the Lifecycle: The C++ compiler is the only "orchestrator" needed. When an object leaves its scope, its part of the concurrency topography is cleanly dismantled.

### 2. The Distributed Tissue (Glossary)
flux_t uses an evocative terminology to describe its internal "anatomy":

* Pulse (Data): The unit of information flowing through the synapse.
* Vessel (Configuration): The container of potential (Capacity).
* Conduit (Hardware): The path of execution (Thread).
* Valve (Control): The regulator of flow (Gatekeeper).
* Nexus (Routing): The point of convergence and distribution (Synapse).
* Catalyst (Logic): The "distributed tissue"—handlers that plug into the topography to react to pulses.

### 3. Core Architecture
flux_t relies on a strictly layered hierarchy. Each component owns or references the layer beneath it, ensuring a safe and predictable teardown:

* vessel_t: Defines the requested thread capacity.
* conduits_t & conduit_internal_t: The engine. conduits_t allocates the worker threads, each managing its own std::thread and mutex-guarded task queue.
* valve_t: Provides explicit control to toggle flow (open_t, close_t, or drain_t).
* nexus_t: The central router. It dispatches pulses via O(1) modulo hashing and maintains the handler registry.
* hub_t & catalysts_t: RAII boundaries that bind message handlers to a specific hierarchical lifetime.
* reaction_t: User-defined logic. They register with the nexus_t via the catalysts_t scope on construction and deregister on destruction.

### 4. Truth in Engineering
flux_t prioritizes production-ready reliability over experimental "lock-free" complexity:

* Hashing-Based Routing: Target threads are selected via a deterministic hash (hash % conduit_count), ensuring O(1) dispatch and consistent data affinity.
* Standard Synchronization: Task injection and handler registration are safely guarded by standard std::mutex and std::condition_variable primitives.
* Low-Latency Wakeups: Worker threads use a 32-iteration spin-loop for fast wakeups before yielding to the OS to avoid unnecessary context switches during high-load bursts.
* Reliable Teardown: Teardown is managed through explicit draining and RAII. While stack objects follow LIFO (Last-In, First-Out) destruction, elements in internal vectors follow FIFO (First-In, First-Out). The library coordinates these sequences to ensure system safety.

### 5. Design Principles
flux_t adheres to these strict constraints:
* Header-only (No binaries)
* Pure RAII (No manual start/stop)
* No Global State (Isolated instances)
* No Macros (Pure C++20)
* Type-safe reactions (Compile-time validation)
* ASCII-only & 4-space indentation (Standard compliance)

---

### 6. Examples

#### 1. Quick Start (Hello, Flux)

```cpp
#include <flux_t/flux_system.hpp>
#include <iostream>

struct my_pulse : flux_t::pulse_t<my_pulse> {
    int value;
    explicit my_pulse(int v) : value(v) {}
};

struct printer_t : flux_t::reaction_t<printer_t, my_pulse> {
    using base_t = flux_t::reaction_t<printer_t, my_pulse>;
    explicit printer_t(flux_t::catalysts_t& pool) noexcept : base_t(pool) {}

    void respond_t(const my_pulse& p) {
        std::cout << "[printer] pulse value = " << p.value << std::endl;
    }
};

int main() {
    flux_t::vessel_t vessel{4};           
    flux_t::conduits_t conduits{vessel};  
    flux_t::valve_t valve{conduits};      
    flux_t::nexus_t nexus{valve};         
    flux_t::hub_t hub{nexus};             
    flux_t::catalysts_t pool{hub};        

    printer_t printer{pool};

    valve.open_t();
    nexus.broadcast_t(my_pulse{42}); 
    valve.drain_t(); 

    return 0; // Stack unwinds: printer deregisters -> threads join
}
```

#### 2. Thread Affinity (Partition Keys)
Ensures pulses for the same entity hit the same thread, allowing stateful logic without internal mutexes.

```cpp
struct user_pulse : flux_t::pulse_t<user_pulse> {
    uint64_t user_id;
    std::size_t partition_key_t() const noexcept { return user_id; }
};

struct user_tracker_t : flux_t::reaction_t<user_tracker_t, user_pulse> {
    using base_t = flux_t::reaction_t<user_tracker_t, user_pulse>;
    std::unordered_map<uint64_t, int> actions;

    explicit user_tracker_t(flux_t::catalysts_t& pool) noexcept : base_t(pool) {}

    void respond_t(const user_pulse& p) {
        // Safe without a mutex due to deterministic routing
        actions[p.user_id]++;
    }
};
```

#### 3. Nested Scoping
Handlers automatically deregister when their local catalysts_t scope is destroyed.

```cpp
{
    flux_t::hub_t local_hub{nexus};
    flux_t::catalysts_t local_pool{local_hub};

    temp_handler_t h1{local_pool};
    nexus.broadcast_t(some_pulse{});
} // h1 deregisters here; the parent nexus/conduits remain active.
```

---

### 7. Integration & License
Include the headers and set your project to C++20.

target_include_directories(your_app PRIVATE path/to/flux_t/include)
set_target_properties(your_app PROPERTIES CXX_STANDARD 20)

Distributed under the MIT License. Copyright (c) 2026 Kristof Barta.
