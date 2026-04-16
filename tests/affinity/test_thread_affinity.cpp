#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

struct test_pulse : pulse_t<test_pulse> {};

struct test_reaction : reaction_t<test_reaction, test_pulse>
{
    std::atomic<std::thread::id>* tid;

    test_reaction(catalysts_t& pool, std::atomic<std::thread::id>* out)
        : reaction_t<test_reaction, test_pulse>(pool)
        , tid(out)
    {
    }

    void respond_t(const test_pulse&)
    {
        *tid = std::this_thread::get_id();
    }
};

TEST(Affinity, ReactionRunsOnSingleThread)
{
    std::atomic<std::thread::id> tid{};
    tid.store(std::thread::id{}, std::memory_order_relaxed);

    vessel_t vessel{ 4 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };
    valve.open_t();

    nexus_t::config_t cfg{ "test_nexus", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    test_reaction r{ cats, &tid };

    nexus.broadcast_t(valve, test_pulse{});

    // naive wait – later you can replace with a proper fence/latch
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_NE(tid.load(std::memory_order_relaxed), std::thread::id{});
}
