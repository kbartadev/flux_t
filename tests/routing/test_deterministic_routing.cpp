#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

struct test_pulse : pulse_t<test_pulse> {};

struct test_reaction : reaction_t<test_reaction, test_pulse>
{
    std::atomic<int>* counter;

    test_reaction(catalysts_t& pool, std::atomic<int>* c)
        : reaction_t(pool), counter(c) {
    }

    void respond_t(const test_pulse&)
    {
        counter->fetch_add(1, std::memory_order_relaxed);
    }
};

TEST(Routing, DeterministicRouting)
{
    std::atomic<int> counter{ 0 };

    vessel_t vessel{ 4 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };
    valve.open_t();

    nexus_t::config_t cfg{ "routing_test", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    test_reaction r{ cats, &counter };

    for (int i = 0; i < 100; ++i)
        nexus.broadcast_t(valve, test_pulse{});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_GE(counter.load(), 100);
}
