#pragma once
#include <string>
#include <memory>

namespace academy {

struct Greeter {
    std::string name;

    explicit Greeter(const std::string& n): name(n) {}

    ~Greeter() {}
    void greet() const;
};

std::shared_ptr<Greeter> make_greeter(const std::string& name);

} // namespace academy