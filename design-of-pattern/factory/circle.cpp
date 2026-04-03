#include "circle.hpp"

namespace patterns {

static constexpr double PI = 3.14159265358979323846;

Circle::Circle(double radius) : radius_(radius) {}

std::string Circle::name() const { return "Circle"; }

double Circle::area() const { return PI * radius_ * radius_; }

} // namespace patterns