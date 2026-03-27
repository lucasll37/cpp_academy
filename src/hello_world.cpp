#include "hello_world.hpp"
#include <iostream>

namespace academy {
    void to_greet(const std::string& name) {
        std::cout << "Hello, " << name << std::endl;
    }
}