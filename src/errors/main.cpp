// =============================================================================
//  src/errors/main.cpp
//  Ponto de entrada — executa todos os demos de tratamento de erro
//
//  ⚠ Este subprojeto compila com C++23 (por causa de std::expected).
//    O padrão da raiz continua sendo C++20 — ver meson.build local.
// =============================================================================
#include "demo_exceptions.hpp"
#include "demo_exception_safety.hpp"
#include "demo_hierarchy.hpp"
#include "demo_error_code.hpp"
#include "demo_expected.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

#include <exception>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::indian_red) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    // Um dos três lugares legítimos para `catch (...)`: a fronteira do programa.
    try {
        section("1 · Exceções — throw, unwinding, rethrow");
        demo_exceptions::run();

        section("2 · Garantias de exceção e noexcept");
        demo_exception_safety::run();

        section("3 · A hierarquia padrão e como estender");
        demo_hierarchy::run();

        section("4 · std::error_code — erros como valores");
        demo_error_code::run();

        section("5 · std::expected<T,E> — C++23");
        demo_expected::run();
    }
    catch (const std::exception& e) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
                   "\n✗ exceção escapou dos demos: {}\n", e.what());
        return 1;
    }
    catch (...) {
        fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold,
                   "\n✗ exceção de tipo desconhecido escapou dos demos\n");
        return 2;
    }

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de tratamento de erro concluídos!\n\n");
    return 0;
}
