#include "logger.hpp"
#include <fmt/core.h>

namespace patterns {

void Logger::on_event(const Event& event) {
    fmt::println("[LOG] {}: {}", event.source, event.message);
}

} // namespace patterns