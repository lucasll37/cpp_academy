#include "greeter.hpp"
#include <fmt/core.h>

namespace academy {

void Greeter::greet() const {
    fmt::println("Hello, {}", name);
}

std::shared_ptr<Greeter> make_greeter(const std::string& name) {
    return std::make_shared<Greeter>(name);
}

} // namespace academy