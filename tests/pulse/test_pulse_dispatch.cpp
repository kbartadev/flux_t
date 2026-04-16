#include <gtest/gtest.h>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

struct p1 : pulse_t<p1> {};
struct p2 : pulse_t<p2> {};

struct multi_reaction : reaction_t<multi_reaction, p1, p2>
{
    int* c1;
    int* c2;

    multi_reaction(catalysts_t& pool, int* a, int* b)
        : reaction_t<multi_reaction, p1, p2>(pool)
        , c1(a)
        , c2(b)
    {
    }

    void respond_t(const p1&) { (*c1)++; }
    void respond_t(const p2&) { (*c2)++; }
};

TEST(Pulse, MultiPulseDispatch)
{
    int c1 = 0;
    int c2 = 0;

    vessel_t vessel{ 1 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };
    valve.open_t();

    nexus_t::config_t cfg{ "dispatch_test", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    multi_reaction r{ cats, &c1, &c2 };

    nexus.broadcast_t(valve, p1{});
    nexus.broadcast_t(valve, p2{});

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    EXPECT_EQ(c1, 1);
    EXPECT_EQ(c2, 1);
}
