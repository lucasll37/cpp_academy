// =============================================================================
//  src/casts/demo_c_style.hpp
//
//  Cast estilo C — (T)expr — e por que evitar em C++ moderno
//  ──────────────────────────────────────────────────────────
//  • Sintaxe herdada de C: (TipoDestino) expressao
//  • Tenta silenciosamente, na ordem:
//      1. const_cast
//      2. static_cast
//      3. static_cast + const_cast
//      4. reinterpret_cast
//      5. reinterpret_cast + const_cast
//  • O compilador escolhe o primeiro que "funciona" — sem avisar qual usou.
//  • Consequência: pode fazer coisas perigosas silenciosamente.
//  • Regra: NUNCA use em código C++ novo. Use os casts nomeados.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdint>

namespace demo_c_style {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O básico — parece inofensivo, mas esconde a intenção
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 5.1 Cast C estilo básico ──\n");

    double pi = 3.14159;

    int c_style  = (int)pi;                    // C-style cast
    int cpp_cast = static_cast<int>(pi);       // equivalente em C++

    fmt::print("  C-style  (int)3.14 = {}\n", c_style);
    fmt::print("  C++ cast           = {}\n", cpp_cast);
    fmt::print("  Resultado idêntico — mas o C++ comunica a intenção claramente\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O perigo — remove const silenciosamente sem avisar
// ─────────────────────────────────────────────────────────────────────────────
void demo_const_silencioso() {
    fmt::print("\n── 5.2 C-style remove const silenciosamente ──\n");

    const int limite = 100;

    // C-style: faz const_cast implicitamente, sem nenhum aviso!
    int* ptr_c = (int*)&limite;
    fmt::print("  (int*)&limite compilou sem aviso!\n");
    fmt::print("  *ptr_c = {} (leitura OK, escrita seria UB)\n", *ptr_c);

    // C++ nomeado: o compilador avisa/rejeita se você usar static_cast errado
    // int* ptr_cpp = static_cast<int*>(&limite); // ✗ ERRO de compilação!
    // Você seria FORÇADO a usar const_cast, que torna a intenção explícita:
    int* ptr_cpp = const_cast<int*>(&limite);  // ao menos você sabe o que está fazendo
    fmt::print("  const_cast explícito: intenção clara para o leitor\n");
    (void)ptr_cpp;
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O perigo maior — faz reinterpret sem avisar
// ─────────────────────────────────────────────────────────────────────────────
struct Vec2 { float x, y; };
struct Cor  { uint8_t r, g, b, a; };

void demo_reinterpret_silencioso() {
    fmt::print("\n── 5.3 C-style faz reinterpret_cast silenciosamente ──\n");

    Vec2 ponto { 1.0f, 2.0f };

    // C-style: compila sem aviso, mas reinterpreta bytes de Vec2 como Cor!
    Cor* cor_c = (Cor*)&ponto;
    fmt::print("  Vec2{{1.0,2.0}} reinterpretado como Cor: r={} g={} b={} a={}\n",
               cor_c->r, cor_c->g, cor_c->b, cor_c->a);
    fmt::print("  (resultado sem sentido — mas compilou silenciosamente!)\n");

    // Com reinterpret_cast: a intenção violenta fica ÓBVIA no código
    // Cor* cor_cpp = reinterpret_cast<Cor*>(&ponto); // ao menos é explícito
    fmt::print("  Com reinterpret_cast nomeado, o revisor vê imediatamente o risco\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Buscabilidade — por que casts nomeados são melhores para manutenção
// ─────────────────────────────────────────────────────────────────────────────
void demo_buscabilidade() {
    fmt::print("\n── 5.4 Buscabilidade no código ──\n");
    fmt::print(
        "  Imagine 100.000 linhas de código com:\n"
        "  • C-style cast (int): aparece em init de variável, arrays,\n"
        "    lambdas, parâmetros — impossível encontrar só os casts\n\n"
        "  • static_cast<int>: grep/ripgrep encontra EXATAMENTE os casts\n"
        "    → grep -n 'static_cast' src/**/*.cpp\n"
        "    → grep -n 'reinterpret_cast' src/**/*.cpp  ← pontos de risco!\n\n"
        "  Em revisão de código: cada reinterpret_cast merece atenção.\n"
        "  C-style cast esconde esse sinal completamente.\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Tabela comparativa final
// ─────────────────────────────────────────────────────────────────────────────
void demo_comparativo() {
    fmt::print("\n── 5.5 Tabela comparativa de todos os casts ──\n");
    fmt::print(
        "  ┌──────────────────────┬───────────┬──────────┬─────────────────────────────────┐\n"
        "  │ Cast                 │ Compile   │ Runtime  │ Uso principal                   │\n"
        "  ├──────────────────────┼───────────┼──────────┼─────────────────────────────────┤\n"
        "  │ static_cast          │ verificado│ nenhum   │ numérico, up/downcast certo      │\n"
        "  │ dynamic_cast         │ verificado│ RTTI     │ downcast seguro, crosscast       │\n"
        "  │ const_cast           │ verificado│ nenhum   │ add/remove const (APIs legadas)  │\n"
        "  │ reinterpret_cast     │ nenhum    │ nenhum   │ bytes brutos, ptr↔int, callbacks │\n"
        "  │ (T) C-style          │ nenhum    │ nenhum   │ EVITAR — obscuro e perigoso      │\n"
        "  └──────────────────────┴───────────┴──────────┴─────────────────────────────────┘\n"
    );

    fmt::print(
        "\n  Hierarquia de preferência (do mais seguro ao menos):\n"
        "  1. Sem cast          — melhor de todos\n"
        "  2. static_cast       — conversão explícita verificada\n"
        "  3. dynamic_cast      — downcast com verificação em runtime\n"
        "  4. const_cast        — apenas para interop com código legado\n"
        "  5. reinterpret_cast  — apenas quando realmente necessário\n"
        "  6. (T) C-style       — nunca em C++ moderno\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_const_silencioso();
    demo_reinterpret_silencioso();
    demo_buscabilidade();
    demo_comparativo();
}

} // namespace demo_c_style
