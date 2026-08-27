// =============================================================================
//  src/linkage/demo_traducao.hpp
//
//  A unidade de tradução
//  ─────────────────────
//  O C++ não compila "o projeto". Ele compila UMA unidade de tradução por vez,
//  em total ignorância das outras. Uma unidade de tradução (TU) é:
//
//      um .cpp  +  tudo que os #include dele trouxeram, recursivamente
//
//  Cada TU vira um .o com uma lista de símbolos DEFINIDOS e uma lista de
//  símbolos NECESSÁRIOS. O linker cola tudo no fim. Praticamente todo erro
//  confuso de build ("undefined reference", "multiple definition",
//  "duplicate symbol") é uma consequência direta desse modelo.
//
//  Tópicos:
//  • O pipeline: pré-processador → compilador → linker
//  • Quanto código um #include realmente traz
//  • Include guards vs #pragma once
//  • ⚠ Onde cada erro clássico aparece
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>

namespace demo_traducao {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O pipeline
// ─────────────────────────────────────────────────────────────────────────────
void demo_pipeline() {
    fmt::print("\n── 1.1 O pipeline ──\n");

    fmt::print("      main.cpp ─┐\n");
    fmt::print("      a.hpp ────┼→ [pré-processador] → uma TU (texto gigante)\n");
    fmt::print("      b.hpp ────┘         g++ -E\n");
    fmt::print("                             ↓\n");
    fmt::print("                        [compilador]  → main.o  (símbolos)\n");
    fmt::print("                          g++ -c\n");
    fmt::print("                             ↓\n");
    fmt::print("      outra.o, libfmt.a ─→ [linker]   → executável\n");
    fmt::print("                              ld\n\n");

    fmt::print("      Para ver cada etapa:\n");
    fmt::print("        g++ -E  arq.cpp        # só o pré-processador\n");
    fmt::print("        g++ -S  arq.cpp        # até o assembly\n");
    fmt::print("        g++ -c  arq.cpp        # até o objeto\n");
    fmt::print("        nm -C arq.o            # os símbolos, desmanglados\n");
    fmt::print("        nm -C --undefined-only arq.o   # o que ele PRECISA\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Quanto código um #include traz
// ─────────────────────────────────────────────────────────────────────────────
void demo_tamanho() {
    fmt::print("\n── 1.2 O que um #include custa ──\n");

    fmt::print("      Linhas depois do pré-processador, MEDIDAS neste toolchain\n");
    fmt::print("      (GCC 13.3, -std=c++20) com um arquivo que só inclui:\n\n");
    fmt::print("        arquivo vazio       →        6 linhas\n");
    fmt::print("        <cstddef>           →      253\n");
    fmt::print("        <vector>            →   26.258\n");
    fmt::print("        <algorithm>         →   28.270\n");
    fmt::print("        <string>            →   31.825\n");
    fmt::print("        <iostream>          →   43.165\n");
    fmt::print("        <ranges>            →   44.309\n\n");
    fmt::print("      Reproduza — e confira, porque os números mudam com a\n");
    fmt::print("      versão da libstdc++:\n");
    fmt::print("         for h in cstddef vector string ranges; do\n");
    fmt::print("           echo \"#include <$h>\" > /tmp/t.cpp\n");
    fmt::print("           printf '%-12s %s\\n' \"$h\" \"$(g++ -std=c++20 -E /tmp/t.cpp | wc -l)\"\n");
    fmt::print("         done\n\n");
    fmt::print("      (a primeira versão desta tabela era chutada; cinco dos seis\n");
    fmt::print("       números estavam errados, `<cstddef>` por um fator de 4)\n\n");

    fmt::print("      Cada TU do projeto paga esse custo INTEIRO, de novo. Um\n");
    fmt::print("      projeto com 100 .cpp que incluem <string> compila\n");
    fmt::print("      <string> cem vezes. É por isso que build de C++ é lento.\n\n");

    fmt::print("      As três saídas, em ordem de eficácia:\n");
    fmt::print("        1. MÓDULOS (C++20) — a interface é compilada uma vez\n");
    fmt::print("           e importada em forma binária. Ver src/modules/.\n");
    fmt::print("        2. declaração adiantada (`class Widget;`) em vez de\n");
    fmt::print("           `#include \"widget.hpp\"` no cabeçalho, quando só se\n");
    fmt::print("           usa ponteiro ou referência.\n");
    fmt::print("        3. pimpl — o cabeçalho não inclui nada; ver seção 5.\n");
    fmt::print("      (precompiled headers e unity builds atacam o sintoma)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Include guards vs #pragma once
// ─────────────────────────────────────────────────────────────────────────────
void demo_guards() {
    fmt::print("\n── 1.3 Include guards vs #pragma once ──\n");

    fmt::print("      Sem proteção, incluir o mesmo cabeçalho duas vezes na mesma\n");
    fmt::print("      TU redefine tudo — `error: redefinition of 'struct X'`.\n\n");

    fmt::print("      | forma            | padrão? | pega hardlink/symlink | risco       |\n");
    fmt::print("      |------------------|---------|-----------------------|-------------|\n");
    fmt::print("      | #ifndef X_H ...  |   SIM   | sim (é o conteúdo)    | colisão de  |\n");
    fmt::print("      |                  |         |                       | nome        |\n");
    fmt::print("      | #pragma once     |   não   | depende do compilador | nenhum na   |\n");
    fmt::print("      |                  |         |                       | prática     |\n\n");

    fmt::print("      `#pragma once` não está no padrão, mas GCC, Clang, MSVC,\n");
    fmt::print("      ICC e mais o suportam há décadas. Este repositório o usa\n");
    fmt::print("      em todos os .hpp; `c_api.h` usa include guard porque\n");
    fmt::print("      também é compilado como C e segue a convenção de lá.\n\n");

    fmt::print("      ⚠  Guards protegem contra inclusão dupla na MESMA TU. Eles\n");
    fmt::print("         NÃO protegem contra ODR — o mesmo cabeçalho é incluído\n");
    fmt::print("         em cada TU, e o que acontece então é o assunto da\n");
    fmt::print("         seção 2.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ Onde cada erro clássico aparece
// ─────────────────────────────────────────────────────────────────────────────
void demo_erros() {
    fmt::print("\n── 1.4 ⚠ Os erros clássicos e sua etapa ──\n");

    fmt::print("      | mensagem                       | etapa       | causa típica       |\n");
    fmt::print("      |--------------------------------|-------------|--------------------|\n");
    fmt::print("      | No such file or directory      | pré-proc.   | -I faltando        |\n");
    fmt::print("      | 'X' was not declared           | compilador  | #include faltando  |\n");
    fmt::print("      | invalid use of incomplete type | compilador  | só a fwd decl      |\n");
    fmt::print("      | undefined reference to 'f()'   | LINKER      | declarou, não      |\n");
    fmt::print("      |                                |             | definiu            |\n");
    fmt::print("      | multiple definition of 'f()'   | LINKER      | função não-inline  |\n");
    fmt::print("      |                                |             | em cabeçalho       |\n");
    fmt::print("      | undefined reference to vtable  | LINKER      | primeira virtual   |\n");
    fmt::print("      |                                |             | não-pura sem corpo |\n\n");

    fmt::print("      Reconhecer a ETAPA já resolve metade do problema:\n");
    fmt::print("        • erro do compilador → falta uma DECLARAÇÃO\n");
    fmt::print("        • erro do linker     → falta uma DEFINIÇÃO (ou sobra)\n\n");

    fmt::print("      \"undefined reference to vtable for X\" merece explicação: o\n");
    fmt::print("      GCC emite a vtable de X na TU que define a PRIMEIRA função\n");
    fmt::print("      virtual não-inline e não-pura (a \"key function\"). Se você\n");
    fmt::print("      declarou `virtual void f();` e nunca a definiu, a vtable\n");
    fmt::print("      não é emitida em lugar nenhum.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_pipeline();
    demo_tamanho();
    demo_guards();
    demo_erros();
}

}  // namespace demo_traducao
