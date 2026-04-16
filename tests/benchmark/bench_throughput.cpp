#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

struct bench_pulse : pulse_t<bench_pulse> {};

struct bench_reaction : reaction_t<bench_reaction, bench_pulse>
{
    bench_reaction(catalysts_t& pool)
        : reaction_t(pool) {
    }

    void respond_t(const bench_pulse&) {}
};

TEST(Benchmark, Throughput)
{
    vessel_t vessel{ 4 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };
    valve.open_t();

    nexus_t::config_t cfg{ "bench_test", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    bench_reaction r{ cats };

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i)
        nexus.broadcast_t(valve, bench_pulse{});

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    SUCCEED();
}
