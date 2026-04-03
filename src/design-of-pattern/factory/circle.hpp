#pragma once
#include "shape.hpp"

namespace patterns {

struct Circle : Shape {
    explicit Circle(double radius);
    std::string name() const override;
    double area() const override;
private:
    double radius_;
};

} // namespace patterns