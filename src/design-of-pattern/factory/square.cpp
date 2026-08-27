#include "square.hpp"

namespace patterns {

Square::Square(double side) : side_(side) {}

std::string Square::name() const { return "Square"; }

double Square::area() const { return side_ * side_; }

} // namespace patterns