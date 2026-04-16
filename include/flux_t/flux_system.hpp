#pragma once

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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <typeindex>
#include <utility>
#include <vector>
#include <stdexcept>
#include <string>

namespace flux_t
{

    // ============================================================================
    // 1. CORE DATA PRIMITIVES
    // ============================================================================

    /// Base type for all pulses flowing through the system.
    struct pulse_base_t
    {
        virtual ~pulse_base_t() = default;
        virtual std::type_index signature_t() const noexcept = 0;
    };

    /// CRTP pulse: derive from this to define a concrete pulse type.
    template<typename derived_t>
    struct pulse_t : pulse_base_t
    {
        std::type_index signature_t() const noexcept override
        {
            return typeid(derived_t);
        }
    };

    /// Ancestry bundle: references to external resources a reaction depends on.
    template<typename... requirements_t>
    struct ancestry_t
    {
        std::tuple<requirements_t&...> resources_t;

        explicit ancestry_t(requirements_t&... refs) noexcept
            : resources_t(refs...)
        {
        }
    };

    // ============================================================================
    // 2. INTERNAL CONDUIT MECHANICS (ENGINE)
    // ============================================================================

    using task_t = std::function<void()>;

    /// Single worker conduit: queue + worker thread + inertia tracking.
    class conduit_internal_t
    {
    public:
        conduit_internal_t()
        {
            worker_thread_t = std::thread([this] { process_flow_t(); });
        }

        conduit_internal_t(const conduit_internal_t&) = delete;
        conduit_internal_t& operator=(const conduit_internal_t&) = delete;
        conduit_internal_t(conduit_internal_t&&) = delete;
        conduit_internal_t& operator=(conduit_internal_t&&) = delete;

        ~conduit_internal_t()
        {
            {
                std::lock_guard<std::mutex> lock(gatekeeper_t);
                is_flowing_t.store(false, std::memory_order_release);
            }
            flow_signal_t.notify_one();
            if (worker_thread_t.joinable())
            {
                worker_thread_t.join();
            }
        }

        /// Inject a task into this conduit.
        void inject_t(task_t task)
        {
            {
                std::lock_guard<std::mutex> lock(gatekeeper_t);
                if (!is_flowing_t.load(std::memory_order_acquire))
                {
                    return;
                }
                flow_queue_t.push(std::move(task));
            }
            inertia_load_t.fetch_add(1, std::memory_order_release);
            flow_signal_t.notify_one();
        }

        /// Current number of in-flight tasks.
        std::size_t get_inertia_t() const noexcept
        {
            return inertia_load_t.load(std::memory_order_acquire);
        }

        /// Block until the queue is empty.
        void drain_t()
        {
            std::unique_lock<std::mutex> lock(gatekeeper_t);
            flow_signal_t.wait(lock, [this] { return flow_queue_t.empty(); });
        }

        /// Stop accepting new tasks and let the worker exit once drained.
        void close_t()
        {
            std::lock_guard<std::mutex> lock(gatekeeper_t);
            is_flowing_t.store(false, std::memory_order_release);
        }

    private:
        void process_flow_t()
        {
            for (;;)
            {
                task_t active_task_t;
                {
                    std::unique_lock<std::mutex> lock(gatekeeper_t);
                    flow_signal_t.wait(lock, [this] {
                        return !flow_queue_t.empty() ||
                            !is_flowing_t.load(std::memory_order_acquire);
                        });

                    if (flow_queue_t.empty() &&
                        !is_flowing_t.load(std::memory_order_acquire))
                    {
                        // Shutdown requested and queue is empty.
                        return;
                    }

                    active_task_t = std::move(flow_queue_t.front());
                    flow_queue_t.pop();
                }

                active_task_t();
                inertia_load_t.fetch_sub(1, std::memory_order_release);
                flow_signal_t.notify_all();
            }
        }

        std::queue<task_t> flow_queue_t;
        std::mutex gatekeeper_t;
        std::condition_variable flow_signal_t;
        std::atomic<std::size_t> inertia_load_t{ 0 };
        std::atomic<bool> is_flowing_t{ true };
        std::thread worker_thread_t;
    };

    // ============================================================================
    // 3. INFRASTRUCTURE & TOPOLOGY (RAII PYRAMID)
    // ============================================================================

    /// Vessel: top-level capacity descriptor for a local execution topology.
    class vessel_t
    {
    public:
        explicit vessel_t(std::size_t cap) noexcept
            : capacity_t(cap)
        {
        }

        vessel_t(const vessel_t&) = delete;
        vessel_t& operator=(const vessel_t&) = delete;
        vessel_t(vessel_t&&) = delete;
        vessel_t& operator=(vessel_t&&) = delete;

        std::size_t get_capacity_t() const noexcept
        {
            return capacity_t;
        }

    private:
        std::size_t capacity_t;
    };

    /// Conduits: fixed set of worker conduits owned by a vessel.
    class conduits_t
    {
    public:
        explicit conduits_t(vessel_t& parent)
        {
            for (std::size_t i = 0; i < parent.get_capacity_t(); ++i)
            {
                channels_t.push_back(std::make_unique<conduit_internal_t>());
            }
        }

        conduits_t(const conduits_t&) = delete;
        conduits_t& operator=(const conduits_t&) = delete;
        conduits_t(conduits_t&&) = delete;
        conduits_t& operator=(conduits_t&&) = delete;

        std::size_t size_t_() const noexcept
        {
            return channels_t.size();
        }

        conduit_internal_t& get_t(std::size_t index)
        {
            return *channels_t[index];
        }

    private:
        std::vector<std::unique_ptr<conduit_internal_t>> channels_t;
    };

    /// Valve: flow control over a conduits fabric (open/close/drain).
    class valve_t
    {
    public:
        explicit valve_t(conduits_t& conduits) noexcept
            : target_t(conduits)
        {
        }

        valve_t(const valve_t&) = delete;
        valve_t& operator=(const valve_t&) = delete;
        valve_t(valve_t&&) = delete;
        valve_t& operator=(valve_t&&) = delete;

        void open_t() noexcept
        {
            is_open_t.store(true, std::memory_order_release);
        }

        void close_t() noexcept
        {
            is_open_t.store(false, std::memory_order_release);
            for (std::size_t i = 0; i < target_t.size_t_(); ++i)
            {
                target_t.get_t(i).close_t();
            }
        }

        void drain_t() noexcept
        {
            for (std::size_t i = 0; i < target_t.size_t_(); ++i)
            {
                target_t.get_t(i).drain_t();
            }
        }

        bool check_flow_t() const noexcept
        {
            return is_open_t.load(std::memory_order_acquire);
        }

    private:
        conduits_t& target_t;
        std::atomic<bool> is_open_t{ false };
    };

    // ============================================================================
    // 4. DISTRIBUTED ROUTING & CATALYST REGISTRY (NEXUS)
    // ============================================================================

    class catalyst_base_t;

    /// Nexus: routing entry point and catalyst registry.
    class nexus_t
    {
    public:
        struct config_t
        {
            std::string identity_t;
            std::vector<std::string> lattice_t;
        };

        explicit nexus_t(conduits_t& conduits, config_t /*cfg*/) noexcept
            : local_conduits_t(conduits)
        {
        }

        nexus_t(const nexus_t&) = delete;
        nexus_t& operator=(const nexus_t&) = delete;
        nexus_t(nexus_t&&) = delete;
        nexus_t& operator=(nexus_t&&) = delete;

        void bind_catalyst_t(catalyst_base_t* cat_t)
        {
            std::lock_guard<std::mutex> lock(registry_lock_t);
            local_catalysts_t.push_back(cat_t);
        }

        void unbind_catalyst_t(catalyst_base_t* cat_t)
        {
            std::lock_guard<std::mutex> lock(registry_lock_t);
            for (auto it = local_catalysts_t.begin();
                it != local_catalysts_t.end();
                ++it)
            {
                if (*it == cat_t)
                {
                    local_catalysts_t.erase(it);
                    break;
                }
            }
        }

        /// Broadcast a pulse using gravity routing (least loaded conduit).
        template<typename derived_pulse_t>
        void broadcast_t(const valve_t& valve, derived_pulse_t pulse)
        {
            if (!valve.check_flow_t())
            {
                return;
            }

            std::size_t best_index_t = 0;
            std::size_t min_load_t = static_cast<std::size_t>(-1);

            for (std::size_t i = 0; i < local_conduits_t.size_t_(); ++i)
            {
                std::size_t load = local_conduits_t.get_t(i).get_inertia_t();
                if (load < min_load_t)
                {
                    min_load_t = load;
                    best_index_t = i;
                }
            }

            local_conduits_t.get_t(best_index_t).inject_t(
                [this, pulse_copy = std::move(pulse)]() {
                    std::lock_guard<std::mutex> lock(registry_lock_t);
                    for (auto* cat : local_catalysts_t)
                    {
                        cat->react_t(pulse_copy);
                    }
                });
        }

    private:
        conduits_t& local_conduits_t;
        std::mutex registry_lock_t;
        std::vector<catalyst_base_t*> local_catalysts_t;
    };

    /// Hub: domain boundary over a nexus.
    class hub_t
    {
    public:
        explicit hub_t(nexus_t& n) noexcept
            : parent_nexus_t(n)
        {
        }

        hub_t(const hub_t&) = delete;
        hub_t& operator=(const hub_t&) = delete;
        hub_t(hub_t&&) = delete;
        hub_t& operator=(hub_t&&) = delete;

        nexus_t& get_nexus_t() const noexcept
        {
            return parent_nexus_t;
        }

    private:
        nexus_t& parent_nexus_t;
    };

    /// Catalysts: RAII handle for registering reactions into a hub.
    class catalysts_t
    {
    public:
        explicit catalysts_t(hub_t& h) noexcept
            : parent_hub_t(h)
        {
        }

        catalysts_t(const catalysts_t&) = delete;
        catalysts_t& operator=(const catalysts_t&) = delete;
        catalysts_t(catalysts_t&&) = delete;
        catalysts_t& operator=(catalysts_t&&) = delete;

        nexus_t& get_nexus_t() const noexcept
        {
            return parent_hub_t.get_nexus_t();
        }

    private:
        hub_t& parent_hub_t;
    };

    // ============================================================================
    // 5. BEHAVIORAL INTERFACES (REACTION MODEL)
    // ============================================================================

    /// Base class for all catalysts registered into a catalysts_t pool.
    class catalyst_base_t
    {
    public:
        explicit catalyst_base_t(catalysts_t& pool_t) noexcept
            : pool_ref_t(pool_t)
        {
            pool_ref_t.get_nexus_t().bind_catalyst_t(this);
        }

        catalyst_base_t(const catalyst_base_t&) = delete;
        catalyst_base_t& operator=(const catalyst_base_t&) = delete;
        catalyst_base_t(catalyst_base_t&&) = delete;
        catalyst_base_t& operator=(catalyst_base_t&&) = delete;

        virtual ~catalyst_base_t()
        {
            pool_ref_t.get_nexus_t().unbind_catalyst_t(this);
        }

        virtual void react_t(const pulse_base_t& signal_t) = 0;

    protected:
        catalysts_t& pool_ref_t;
    };

    /// CRTP reaction: type-safe dispatch over handled pulse types.
    template<typename derived_t, typename... handled_pulses_t>
    class reaction_t : public catalyst_base_t
    {
    public:
        explicit reaction_t(catalysts_t& pool_t) noexcept
            : catalyst_base_t(pool_t)
        {
        }

        void react_t(const pulse_base_t& signal_t) final
        {
            (attempt_reaction_t<handled_pulses_t>(signal_t) || ...);
        }

    private:
        template<typename specific_pulse_t>
        bool attempt_reaction_t(const pulse_base_t& signal_t)
        {
            if (signal_t.signature_t() == typeid(specific_pulse_t))
            {
                static_cast<derived_t*>(this)->respond_t(
                    static_cast<const specific_pulse_t&>(signal_t));
                return true;
            }
            return false;
        }
    };

} // namespace flux_t
