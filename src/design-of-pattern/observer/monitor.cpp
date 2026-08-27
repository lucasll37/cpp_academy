#include "monitor.hpp"
#include <fmt/core.h>

namespace patterns {

void Monitor::on_event(const Event& event) {
    history_.push_back(event);
    fmt::println("[MONITOR] evento #{}: {}", history_.size(), event.message);
}

const std::vector<Event>& Monitor::history() const {
    return history_;
}

} // namespace patterns