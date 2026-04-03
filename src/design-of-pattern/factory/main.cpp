#include "shape_factory.hpp"
#include <fmt/core.h>
#include <vector>
#include <memory>

int main() {
    std::vector<std::unique_ptr<patterns::Shape>> shapes;

    shapes.push_back(patterns::ShapeFactory::create("circle", 5.0));
    shapes.push_back(patterns::ShapeFactory::create("square", 4.0));

    for (const auto& s : shapes) {
        fmt::println("{}: area = {:.2f}", s->name(), s->area());
    }

    try {
        patterns::ShapeFactory::create("triangle", 3.0);
    } catch (const std::invalid_argument& e) {
        fmt::println("Erro esperado: {}", e.what());
    }

    return 0;
}