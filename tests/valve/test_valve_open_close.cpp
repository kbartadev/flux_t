#include <gtest/gtest.h>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

TEST(Valve, OpenCloseAndDrain)
{
    vessel_t vessel{ 2 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };

    EXPECT_FALSE(valve.check_flow_t());
    valve.open_t();
    EXPECT_TRUE(valve.check_flow_t());

    valve.close_t();
    EXPECT_FALSE(valve.check_flow_t());

    // drain_t should be safe even after close
    valve.drain_t();
}
