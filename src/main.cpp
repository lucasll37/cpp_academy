#include "greeter.hpp"
// #include "common.pb.h"

#include <fmt/core.h>

int main() {
    std::shared_ptr<academy::Greeter> a;
    // mlinference::common::Test b;
    // b = mlinference::common::Test::BOOL;

    {
        auto b = academy::make_greeter("Lucas");
        fmt::println("use_count dentro do bloco: ", b.use_count());

        a = b; // compartilha propriedade
        fmt::println("use_count após compartilhar: ", b.use_count());

        b->greet();
    } // b sai de escopo → use_count vai para 1, objeto NÃO é destruído

    fmt::println("use_count após bloco: ", a.use_count());
    a->greet(); // ainda válido!

    // a sai de escopo → use_count vai para 0 → objeto destruído aqui
    return 0;
}