#include <gtest/gtest.h>
#include <atomic>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

struct dummy_pulse : pulse_t<dummy_pulse> {};

struct dummy_reaction : reaction_t<dummy_reaction, dummy_pulse>
{
    std::atomic<int>* counter;

    dummy_reaction(catalysts_t& pool, std::atomic<int>* c)
        : reaction_t<dummy_reaction, dummy_pulse>(pool)
        , counter(c)
    {
    }

    void respond_t(const dummy_pulse&)
    {
        counter->fetch_add(1, std::memory_order_relaxed);
    }
};

TEST(Reaction, LifecycleRegistration)
{
    std::atomic<int> counter{ 0 };

    vessel_t vessel{ 1 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };
    valve.open_t();

    nexus_t::config_t cfg{ "lifecycle_test", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    {
        dummy_reaction r{ cats, &counter };
        nexus.broadcast_t(valve, dummy_pulse{});
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
    }

    int before = counter.load(std::memory_order_relaxed);
    nexus.broadcast_t(valve, dummy_pulse{});
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(counter.load(std::memory_order_relaxed), before);
}
