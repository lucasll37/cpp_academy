#include "compression.hpp"
#include <fmt/core.h>

namespace patterns {

void Compression::write(const std::string& data) {
    std::string compressed = "[compressed:" + data + "]";
    fmt::println("  Compression::write  → {}", compressed);
    wrapped_->write(compressed);
}

std::string Compression::read() {
    std::string raw = wrapped_->read();
    // remove os marcadores simulados
    if (raw.size() > 14 && raw.substr(0, 13) == "[compressed:") {
        raw = raw.substr(13, raw.size() - 14);
    }
    fmt::println("  Compression::read   ← {}", raw);
    return raw;
}

} // namespace patterns