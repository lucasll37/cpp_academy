#include "hello_world.hpp"
#include <fmt/core.h>

namespace academy {
    void to_greet(const std::string& name) {
        fmt::println("Hello, {}", name);
    }
}