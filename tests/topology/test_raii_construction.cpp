#include <gtest/gtest.h>
#include "flux_t/flux_system.hpp"

using namespace flux_t;

TEST(Topology, RAIIConstructionPyramid)
{
    vessel_t vessel{ 4 };
    conduits_t conduits{ vessel };
    valve_t valve{ conduits };

    nexus_t::config_t cfg{ "topology_test", {} };
    nexus_t nexus{ conduits, cfg };
    hub_t hub{ nexus };
    catalysts_t cats{ hub };

    SUCCEED(); // if we got here, construction worked
}
