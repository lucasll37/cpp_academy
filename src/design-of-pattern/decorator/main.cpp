#include "file_stream.hpp"
#include "compression.hpp"
#include "encryption.hpp"
#include "logging.hpp"
#include <fmt/core.h>
#include <memory>

using namespace patterns;

int main() {
    // empilha: Logging → Encryption → Compression → FileStream
    auto stream =
        std::make_unique<Logging>(
        std::make_unique<Encryption>(
        std::make_unique<Compression>(
        std::make_unique<FileStream>())));

    fmt::println("=== write ===");
    stream->write("hello academy");

    fmt::println("\n=== read ===");
    std::string result = stream->read();
    fmt::println("\nresult: {}", result);

    return 0;
}