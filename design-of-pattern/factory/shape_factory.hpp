#pragma once
#include "shape.hpp"
#include "circle.hpp"
#include "square.hpp"
#include <memory>
#include <string>
#include <stdexcept>

namespace patterns {

struct ShapeFactory {
    static std::unique_ptr<Shape> create(const std::string& type, double param) {
        if (type == "circle") return std::make_unique<Circle>(param);
        if (type == "square") return std::make_unique<Square>(param);
        throw std::invalid_argument("Unknown shape: " + type);
    }
};

} // namespace patterns