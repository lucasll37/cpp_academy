#pragma once
#include <string>

namespace patterns {

struct Shape {
    virtual ~Shape() = default;
    virtual std::string name() const = 0;
    virtual double area() const = 0;
};

} // namespace patterns