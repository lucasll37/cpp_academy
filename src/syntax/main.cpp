// =============================================================================
//  src/syntax/main.cpp
//  Ponto de entrada — a sintaxe padrão da linguagem, de ponta a ponta
//
//  Este subprojeto é o mapa: cada construção sintática do C++ aparece aqui,
//  executada e comentada. Quando um tópico tem um subprojeto dedicado
//  (templates, concepts, errors, casts, oop, value_semantics...), a seção
//  mostra a SINTAXE e aponta para lá.
//
//  Compila com C++20 (o padrão da raiz).
// =============================================================================
#include "demo_variaveis.hpp"
#include "demo_controle.hpp"
#include "demo_funcoes.hpp"
#include "demo_tipos.hpp"
#include "demo_classes.hpp"
#include "demo_organizacao.hpp"
#include "demo_expressoes.hpp"
#include "demo_generico.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::turquoise) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Declarações — tipos, variáveis e inicialização");
    demo_variaveis::run();

    section("2 · Controle de fluxo — if, switch e loops");
    demo_controle::run();

    section("3 · Funções");
    demo_funcoes::run();

    section("4 · Tipos definidos pelo usuário");
    demo_tipos::run();

    section("5 · Classes");
    demo_classes::run();

    section("6 · Organização — namespaces e pré-processador");
    demo_organizacao::run();

    section("7 · Expressões, operadores e conversões");
    demo_expressoes::run();

    section("8 · Sintaxe genérica e de erros");
    demo_generico::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de sintaxe concluídos!\n\n");
}
