#pragma once
#include "shape.hpp"

namespace patterns {

struct Square : Shape {
    explicit Square(double side);
    std::string name() const override;
    double area() const override;
private:
    double side_;
};

} // namespace patterns