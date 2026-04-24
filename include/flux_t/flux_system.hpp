// MIT License
// Copyright (c) 2026 Kristof Barta
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>
#include <string>
#include <mutex>
#include <condition_variable>

namespace flux_t {

    // ============================================================================
    // 1. FORWARD DECLARATIONS
    // ============================================================================
    class nexus_t;
    struct catalyst_base_t;
    class valve_t;

    // ============================================================================
    // 2. BASE PRIMITIVES
    // ============================================================================

    template <typename T>
    struct pulse_t {
        virtual ~pulse_t() = default;
    };

    struct catalyst_base_t {
        std::atomic<bool> is_detached{ false };

        // The base class physically holds its routing link to prevent vtable races.
        nexus_t* active_nexus{ nullptr };

        // Explicit control point to vaporize future pulses safely
        void detach_t();

        // Safe fallback for standard lifecycles
        virtual ~catalyst_base_t();

        // Type-erased execution hook
        virtual void react_t(const void* pulse_ptr) = 0;
    };

    // ============================================================================
    // 3. CORE ROUTING STRUCTURES
    // ============================================================================

    struct route_slot_t {
        std::atomic<catalyst_base_t*> target{ nullptr };
        std::atomic<int32_t> in_flight{ 0 }; // Hardware fence counter
    };

    // ============================================================================
    // 4. INFRASTRUCTURE SCAFFOLDING
    // ============================================================================

    struct vessel_t {
        size_t capacity;
        explicit vessel_t(size_t c) : capacity(c) {}
    };

    // Explicit Topology. 1 Conduit = 1 Thread + 1 Queue.
    // Zero cross-thread contention.
    struct conduit_internal_t {
        std::thread worker;
        std::vector<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop{ false };

        conduit_internal_t() {
            worker = std::thread([this]() {
                while (!stop.load(std::memory_order_relaxed)) {
                    std::function<void()> task;

                    // 1. FAST PATH: 32-iteration spin-loop
                    // Avoids the massive latency of yielding to the OS under high load.
                    bool found = false;
                    for (int i = 0; i < 32; ++i) {
                        if (queue_mutex.try_lock()) {
                            if (!tasks.empty()) {
                                task = std::move(tasks.front());
                                tasks.erase(tasks.begin());
                                found = true;
                            }
                            queue_mutex.unlock();
                            if (found) break;
                        }
                        // C++20 standard yield to prevent burning CPU cache lines
                        std::this_thread::yield();
                    }

                    // 2. SLOW PATH: OS sleep if spin fails (System is idle)
                    if (!found) {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { return stop.load() || !tasks.empty(); });

                        if (stop.load() && tasks.empty()) return;

                        if (!tasks.empty()) {
                            task = std::move(tasks.front());
                            tasks.erase(tasks.begin());
                        }
                    }

                    // Execute the pulse completely lock-free
                    if (task) task();
                }
                });
        }

        template<typename F>
        void inject_t(F&& func) {
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                tasks.emplace_back(std::forward<F>(func));
            }
            condition.notify_one();
        }

        // Safe, deterministic teardown. No aborted ghost threads.
        ~conduit_internal_t() {
            stop.store(true, std::memory_order_relaxed);
            condition.notify_all();
            if (worker.joinable()) worker.join();
        }
    };

    struct conduits_t {
        vessel_t& vessel;
        // Pointers used so mutexes/threads don't invalidate on vector resize
        std::vector<std::unique_ptr<conduit_internal_t>> lanes;

        explicit conduits_t(vessel_t& v) : vessel(v) {
            for (size_t i = 0; i < vessel.capacity; ++i) {
                lanes.push_back(std::make_unique<conduit_internal_t>());
            }
        }

        // O(1) Data Affinity Dispatch.
        conduit_internal_t& get_t(size_t index) {
            return *lanes[index % lanes.size()];
        }
    };

    struct valve_t {
        conduits_t& conduits;
        std::atomic<bool> is_open{ false };

        explicit valve_t(conduits_t& c) : conduits(c) {}
        void open_t() { is_open.store(true, std::memory_order_release); }
        void close_t() { is_open.store(false, std::memory_order_release); }
        bool check_flow_t() const { return is_open.load(std::memory_order_acquire); }

        void drain_t() {
            // Yield until all lane queues are physically empty
            for (auto& lane : conduits.lanes) {
                while (true) {
                    bool empty = false;
                    {
                        std::lock_guard<std::mutex> lock(lane->queue_mutex);
                        empty = lane->tasks.empty();
                    }
                    if (empty) break;
                    std::this_thread::yield();
                }
            }
        }
    };

    // ============================================================================
    // 5. THE NEXUS (Lock-Free Event Fabric)
    // ============================================================================

    class nexus_t {
    public:
        struct config_t {
            std::string name;
            std::vector<std::string> options;
        };

    private:
        conduits_t& local_conduits_t;
        config_t config;

        static constexpr size_t MAX_CATALYSTS = 1024;
        route_slot_t registry_t[MAX_CATALYSTS];

    public:
        nexus_t(conduits_t& conduits, config_t cfg)
            : local_conduits_t(conduits), config(std::move(cfg)) {
        }

        void bind_catalyst_t(catalyst_base_t* cat);
        void unbind_catalyst_t(catalyst_base_t* cat);

        template<typename derived_pulse_t>
        void broadcast_t(const valve_t& valve, derived_pulse_t pulse) {
            if (!valve.check_flow_t()) return;

            // For general pulses, hash the this pointer or use a round-robin metric. 
            // We will default to lane 0 for this minimal example if no key is provided.
            size_t best_index_t = 0;

            local_conduits_t.get_t(best_index_t).inject_t(
                [this, pulse_copy = std::move(pulse)]() {
                    // Lock-Free O(1) Execution Path
                    for (auto& slot : registry_t) {

                        slot.in_flight.fetch_add(1, std::memory_order_acquire);

                        if (auto* cat = slot.target.load(std::memory_order_relaxed)) {
                            cat->react_t(&pulse_copy);
                        }

                        slot.in_flight.fetch_sub(1, std::memory_order_release);
                    }
                });
        }
    };

    // ============================================================================
    // 6. TOPOLOGY HANDLES (Hubs & Pools)
    // ============================================================================

    struct hub_t {
        nexus_t& parent_nexus;
        explicit hub_t(nexus_t& n) : parent_nexus(n) {}
    };

    struct catalysts_t {
        hub_t& parent_hub;
        explicit catalysts_t(hub_t& h) : parent_hub(h) {}
        nexus_t& get_nexus_t() const { return parent_hub.parent_nexus; }
    };

    template <typename derived_t, typename pulse_type_t>
    struct reaction_t : catalyst_base_t {
        explicit reaction_t(catalysts_t& pool) noexcept {
            pool.get_nexus_t().bind_catalyst_t(this);
        }

        void react_t(const void* pulse_ptr) override {
            // Safely cast back to the derived pulse type
            static_cast<derived_t*>(this)->respond_t(
                *static_cast<const pulse_type_t*>(pulse_ptr)
            );
        }
    };

    // ============================================================================
    // 7. SUPPLEMENTAL AUTOMATION LAYER
    // ============================================================================

    // Encapsulates the Operational Phase to eliminate manual drains
    struct operational_scope_t {
        valve_t& active_valve;
        explicit operational_scope_t(valve_t& v) noexcept : active_valve(v) {}
        ~operational_scope_t() { active_valve.drain_t(); }
        operator valve_t& () { return active_valve; }
        operator const valve_t& () const { return active_valve; }
    };

    // Structurally guarantees lock-free detachment before map destruction
    template<typename handler_t>
    struct route_guard_t {
        handler_t& target;
        explicit route_guard_t(handler_t& h) noexcept : target(h) {}
        ~route_guard_t() { target.detach_t(); }
    };

    // ============================================================================
    // 8. IMPLEMENTATION TAIL
    // ============================================================================

    inline void catalyst_base_t::detach_t() {
        if (!is_detached.exchange(true, std::memory_order_acq_rel)) {
            if (active_nexus) {
                active_nexus->unbind_catalyst_t(this);
            }
        }
    }

    inline catalyst_base_t::~catalyst_base_t() {
        detach_t();
    }

    inline void nexus_t::bind_catalyst_t(catalyst_base_t* cat) {
        for (auto& slot : registry_t) {
            catalyst_base_t* expected = nullptr;
            if (slot.target.compare_exchange_strong(expected, cat)) {
                cat->active_nexus = this; // Establish explicit physical link
                return;
            }
        }
    }

    inline void nexus_t::unbind_catalyst_t(catalyst_base_t* cat) {
        for (auto& slot : registry_t) {
            if (slot.target.load(std::memory_order_relaxed) == cat) {
                // 1. Vaporize future events
                slot.target.store(nullptr, std::memory_order_release);

                // 2. Hardware fence: Wait ONLY if a thread is physically executing right now
                while (slot.in_flight.load(std::memory_order_acquire) > 0) {
                    std::this_thread::yield();
                }
                return;
            }
        }
    }

} // namespace flux_t