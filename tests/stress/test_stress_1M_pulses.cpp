#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

struct stress_pulse : pulse_t<stress_pulse> {};

struct stress_reaction : reaction_t<stress_reaction, stress_pulse>
{
    std::atomic<int>* counter;

    stress_reaction(catalysts_t& pool, std::atomic<int>* c)
        : reaction_t(pool), counter(c) {
    }

    void respond_t(const stress_pulse&)
    {
        counter->fetch_add(1, std::memory_order_relaxed);
    }
};

TEST(Stress, MillionPulses)
{
    std::atomic<int> counter{ 0 };

    vessel_t vessel{ 4 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };
    valve.open_t();

    nexus_t::config_t cfg{ "stress_test", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    stress_reaction r{ cats, &counter };

    for (int i = 0; i < 1'000'000; ++i)
        nexus.broadcast_t(valve, stress_pulse{});

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(counter.load(), 1'000'000);
}
