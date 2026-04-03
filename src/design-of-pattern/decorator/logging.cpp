#include "logging.hpp"
#include <fmt/core.h>

namespace patterns {

void Logging::write(const std::string& data) {
    fmt::println("  Logging::write      → {} chars", data.size());
    wrapped_->write(data);
}

std::string Logging::read() {
    std::string result = wrapped_->read();
    fmt::println("  Logging::read       ← {} chars", result.size());
    return result;
}

} // namespace patterns