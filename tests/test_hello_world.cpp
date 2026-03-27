#include <gtest/gtest.h>
#include "hello_world.hpp"

#include <sstream>
#include <iostream>

// ============================================================
// Utilitário: captura o que foi impresso em std::cout
// ============================================================
namespace test_utils {
    std::string capture_stdout(std::function<void()> fn) {
        std::ostringstream buffer;
        std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
        fn();
        std::cout.rdbuf(old);
        return buffer.str();
    }
} // namespace test_utils

// ============================================================
// Suite: HelloWorld
// ============================================================

// Verifica que a saudação contém o nome passado
TEST(HelloWorldTest, ContainsName) {
    const std::string output = test_utils::capture_stdout([] {
        academy::to_greet("Lucas");
    });
    EXPECT_NE(output.find("Lucas"), std::string::npos);
}

// Verifica o formato exato da saída
TEST(HelloWorldTest, ExactOutput) {
    const std::string output = test_utils::capture_stdout([] {
        academy::to_greet("Lucas");
    });
    EXPECT_EQ(output, "Hello, Lucas\n");
}

// Verifica que a função aceita nomes diferentes
TEST(HelloWorldTest, DifferentNames) {
    const std::string output = test_utils::capture_stdout([] {
        academy::to_greet("World");
    });
    EXPECT_EQ(output, "Hello, World\n");
}

// Verifica comportamento com string vazia
TEST(HelloWorldTest, EmptyName) {
    const std::string output = test_utils::capture_stdout([] {
        academy::to_greet("");
    });
    EXPECT_EQ(output, "Hello, \n");
}

// ============================================================
// Ponto de entrada dos testes
// ============================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}