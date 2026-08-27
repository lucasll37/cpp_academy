// =============================================================================
//  src/linkage/demo_odr.hpp
//
//  A One Definition Rule, medida
//  ─────────────────────────────
//  A ODR tem três partes, e só a terceira costuma surpreender:
//
//    1. em UMA TU, cada entidade pode ser DECLARADA muitas vezes e DEFINIDA
//       no máximo uma;
//    2. no PROGRAMA inteiro, cada função ou variável não-inline pode ser
//       definida no máximo uma vez;
//    3. classes, funções inline e templates PODEM ser definidos em várias
//       TUs — mas as definições precisam ser TOKEN A TOKEN IDÊNTICAS.
//       Se não forem, é UB, e o linker NÃO é obrigado a avisar.
//
//  Esta parte 3 é a origem dos bugs mais difíceis de C++: o programa linka,
//  roda, e faz algo diferente do que o código diz.
//
//  ⚠ Tudo abaixo vem de DOIS arquivos .cpp de verdade — unidade_a.cpp e
//    unidade_b.cpp — compilados separadamente e linkados juntos. Os
//    endereços são do linker, não de simulação.
//
//  Tópicos:
//  • inline: uma definição para o programa inteiro
//  • static / namespace anônimo: uma cópia por TU
//  • ⚠ o `static` em cabeçalho, o bug clássico
//  • Templates e a fusão de instanciações
//  • ⚠ ODR violada sem aviso: o caso da NDEBUG
// =============================================================================
#pragma once
#include "comum.hpp"
#include <fmt/core.h>

namespace demo_odr {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Os endereços, lado a lado
// ─────────────────────────────────────────────────────────────────────────────
void demo_enderecos() {
    fmt::print("\n── 2.1 Os endereços, de duas TUs reais ──\n");

    const auto a = comum::enderecos_de_a();
    const auto b = comum::enderecos_de_b();

    auto linha = [](const char* rotulo, const void* pa, const void* pb) {
        fmt::print("      {:<22} {:<21} {}\n", rotulo,
                   pa == pb ? "MESMO endereço" : "endereços DIFERENTES",
                   pa == pb ? "→ 1 no programa inteiro" : "→ 1 por TU");
    };

    fmt::print("      comparando {} com {}:\n\n", a.tu, b.tu);
    linha("inline int f()",        a.funcao_inline,    b.funcao_inline);
    linha("static int f()",        a.funcao_static,    b.funcao_static);
    linha("namespace{} int f()",   a.funcao_anonima,   b.funcao_anonima);
    linha("inline int var",        a.contador_inline,  b.contador_inline);
    linha("static int var",        a.contador_static,  b.contador_static);
    linha("const int k = 42",      a.constante,        b.constante);
    linha("inline constexpr int",  a.constexpr_inline, b.constexpr_inline);
    linha("template<T> T f(T)",    a.template_int,     b.template_int);

    fmt::print("\n      As três regras que isso demonstra:\n");
    fmt::print("        `inline`            → o linker FUNDE as cópias das TUs;\n");
    fmt::print("        `static` / anônimo  → linkage INTERNA, cada TU tem a sua;\n");
    fmt::print("        `const` em namespace→ é implicitamente `static` em C++\n");
    fmt::print("                              (em C seria externa!);\n");
    fmt::print("        template            → instanciado em cada TU, e as cópias\n");
    fmt::print("                              são fundidas como se fossem inline.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ O `static` em cabeçalho — o bug clássico
// ─────────────────────────────────────────────────────────────────────────────
void demo_static_bug() {
    fmt::print("\n── 2.2 ⚠ `static` em cabeçalho: N variáveis, não 1 ──\n");

    fmt::print("      No comum.hpp existem estas duas:\n");
    fmt::print("        inline int contador_inline = 0;    // C++17\n");
    fmt::print("        static int contador_static = 0;    // ⚠\n\n");

    fmt::print("      Agora cada TU incrementa AS DUAS, 10 vezes cada:\n");
    comum::incrementa_em_a(10);
    comum::incrementa_em_b(10);

    fmt::print("\n      | variável         | vista de A | vista de B |\n");
    fmt::print("      |------------------|------------|------------|\n");
    fmt::print("      | contador_inline  | {:10} | {:10} |\n",
               comum::contador_inline_visto_de_a(),
               comum::contador_inline_visto_de_b());
    fmt::print("      | contador_static  | {:10} | {:10} |\n",
               comum::contador_static_visto_de_a(),
               comum::contador_static_visto_de_b());

    fmt::print("\n      `contador_inline` chegou a 20: é UM objeto, incrementado\n");
    fmt::print("      por duas TUs. É o que quase todo mundo QUER.\n");
    fmt::print("      `contador_static` ficou em 10 dos dois lados: são DUAS\n");
    fmt::print("      variáveis distintas, cada uma vendo só os seus incrementos.\n\n");

    fmt::print("      Este bug é traiçoeiro porque:\n");
    fmt::print("        • linka sem erro nenhum (o GCC só avisa\n");
    fmt::print("          \"defined but not used\" numa TU que não a usa —\n");
    fmt::print("          e num bug de verdade todas as TUs a usam);\n");
    fmt::print("        • funciona perfeitamente enquanto houver UMA TU;\n");
    fmt::print("        • quebra no dia em que alguém inclui o cabeçalho num\n");
    fmt::print("          segundo .cpp — e o sintoma (\"o cache não persiste\")\n");
    fmt::print("          não aponta para o cabeçalho.\n\n");

    fmt::print("      Antes do C++17 a saída era declarar `extern` no cabeçalho e\n");
    fmt::print("      definir em UM .cpp. Desde o C++17, `inline` resolve no\n");
    fmt::print("      próprio cabeçalho. Use `inline`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O que `inline` realmente significa
// ─────────────────────────────────────────────────────────────────────────────
void demo_inline() {
    fmt::print("\n── 2.3 `inline` não é sobre inlining ──\n");

    fmt::print("      A palavra engana desde 1998. `inline` significa:\n\n");
    fmt::print("        \"esta entidade pode ser definida em várias TUs; o linker\n");
    fmt::print("         deve escolher uma e descartar as outras\"\n\n");

    fmt::print("      Ela NÃO obriga o compilador a expandir a chamada no local.\n");
    fmt::print("      Essa decisão é do otimizador, e ele a toma por conta própria\n");
    fmt::print("      mesmo em funções não marcadas (e a IGNORA em funções\n");
    fmt::print("      marcadas que sejam grandes demais).\n\n");

    fmt::print("      | quero...                        | uso                     |\n");
    fmt::print("      |---------------------------------|-------------------------|\n");
    fmt::print("      | definir função em cabeçalho     | inline                  |\n");
    fmt::print("      | definir variável em cabeçalho   | inline (C++17)          |\n");
    fmt::print("      | FORÇAR a expansão               | [[gnu::always_inline]]  |\n");
    fmt::print("      | IMPEDIR a expansão              | [[gnu::noinline]]       |\n\n");

    fmt::print("      São implicitamente inline, sem escrever a palavra:\n");
    fmt::print("        • funções definidas DENTRO do corpo da classe;\n");
    fmt::print("        • funções `constexpr` e `consteval`;\n");
    fmt::print("        • variáveis `static constexpr` membros de classe (C++17);\n");
    fmt::print("        • templates (regra própria, mas efeito parecido).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ ODR violada em silêncio
// ─────────────────────────────────────────────────────────────────────────────
void demo_odr_silenciosa() {
    fmt::print("\n── 2.4 ⚠ A violação que linka e roda ──\n");

    fmt::print("      O caso mais comum, e ele acontece de verdade:\n\n");
    fmt::print("        // widget.hpp\n");
    fmt::print("        struct Widget {{\n");
    fmt::print("            int  a;\n");
    fmt::print("        #ifndef NDEBUG\n");
    fmt::print("            int  contador_de_depuracao;   // ⚠ só em debug\n");
    fmt::print("        #endif\n");
    fmt::print("        }};\n\n");
    fmt::print("        // a.cpp compilado SEM -DNDEBUG → sizeof(Widget) == 8\n");
    fmt::print("        // b.cpp compilado COM  -DNDEBUG → sizeof(Widget) == 4\n\n");

    fmt::print("      As duas TUs discordam sobre o LAYOUT da mesma classe. O\n");
    fmt::print("      linker não compara definições de classe — ele só vê nomes\n");
    fmt::print("      de símbolo. O programa linka, roda, e escreve fora dos\n");
    fmt::print("      limites do objeto quando cruza a fronteira.\n\n");

    fmt::print("      Outras formas da mesma armadilha:\n");
    fmt::print("        • -D diferente entre alvos do build;\n");
    fmt::print("        • #pragma pack ativo em uma TU e não na outra;\n");
    fmt::print("        • -std diferente (o layout de tipos da std pode mudar);\n");
    fmt::print("        • linkar Debug com Release da mesma biblioteca (_GLIBCXX_\n");
    fmt::print("          _ASSERTIONS muda o layout de containers!);\n");
    fmt::print("        • duas versões de uma dependência header-only na mesma\n");
    fmt::print("          árvore.\n\n");

    fmt::print("      Defesas:\n");
    fmt::print("        • flags de compilação UNIFORMES — é para isso que serve o\n");
    fmt::print("          build system (ver src/meson/);\n");
    fmt::print("        • `-flto` faz o compilador ver as duas TUs juntas e às\n");
    fmt::print("          vezes ACUSA (`-Wodr`);\n");
    fmt::print("        • `gold`/`lld` com --detect-odr-violations;\n");
    fmt::print("        • ASan detecta a maioria das consequências\n");
    fmt::print("          (ver src/undefined_behavior/);\n");
    fmt::print("        • NUNCA colocar #if em torno de MEMBRO de classe pública.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_enderecos();
    demo_static_bug();
    demo_inline();
    demo_odr_silenciosa();
}

}  // namespace demo_odr
