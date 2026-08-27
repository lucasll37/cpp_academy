#include "event_bus.hpp"
#include "logger.hpp"
#include "monitor.hpp"
#include <fmt/core.h>

int main() {
    patterns::EventBus bus;
    patterns::Logger   logger;
    patterns::Monitor  monitor;

    bus.subscribe(&logger);
    bus.subscribe(&monitor);

    bus.publish({"sensor-1", "temperatura alta"});
    bus.publish({"sensor-2", "pressão normal"});

    // remove o logger — monitor continua recebendo
    bus.unsubscribe(&logger);

    bus.publish({"sensor-1", "temperatura crítica"});

    fmt::println("\nhistórico completo ({} eventos):", monitor.history().size());
    for (const auto& e : monitor.history()) {
        fmt::println("  {} → {}", e.source, e.message);
    }

    return 0;
}