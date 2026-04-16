// MIT License
// Copyright (c) 2026 Kristof Barta

#include <flux_t/flux_system.hpp>

#include <iostream>
#include <thread>

struct my_pulse : flux_t::pulse_t<my_pulse>
{
    int value{};

    explicit my_pulse(int v) : value(v) {}
};

struct printer_t : flux_t::reaction_t<printer_t, my_pulse>
{
    using base_t = flux_t::reaction_t<printer_t, my_pulse>;

    explicit printer_t(flux_t::catalysts_t& pool) noexcept
        : base_t(pool)
    {
    }

    void respond_t(const my_pulse& p)
    {
        std::cout << "[printer] pulse value = " << p.value << '\n';
    }
};

int main()
{
    using namespace flux_t;

    vessel_t vessel(2);
    conduits_t conduits(vessel);
    valve_t valve(conduits);
    valve.open_t();

    nexus_t::config_t cfg{ "local", {} };
    nexus_t nexus(conduits, cfg);
    hub_t hub(nexus);
    catalysts_t pool(hub);

    printer_t printer(pool);

    my_pulse p{ 42 };
    nexus.broadcast_t(valve, my_pulse{ 42 });

    valve.close_t();
    valve.drain_t();
    return 0;
}
