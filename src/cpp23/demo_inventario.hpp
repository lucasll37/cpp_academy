// =============================================================================
//  src/cpp23/demo_inventario.hpp
//
//  O que este toolchain tem de C++23 — medido, não presumido
//  ─────────────────────────────────────────────────────────
//  "C++23" não é uma coisa que um compilador tem ou não tem. Cada feature
//  chega numa versão diferente, e a biblioteca costuma atrasar em relação à
//  linguagem.
//
//  A forma CORRETA de descobrir o que está disponível são as feature-test
//  macros: cada feature define uma macro `__cpp_*` com a data (AAAAMM) em que
//  foi adotada. Elas vivem em `<version>` (biblioteca) e são predefinidas pelo
//  compilador (linguagem).
//
//  Este demo é a fonte da tabela em src/ROADMAP.md — e duas linhas daquela
//  tabela estavam ERRADAS antes de serem medidas.
//
//  Tópicos:
//  • As feature-test macros e como usá-las
//  • O inventário deste toolchain, gerado em compilação
//  • O padrão de código que degrada com elegância
//  • ⚠ A diferença entre "a macro existe" e "linka"
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <version>

namespace demo_inventario {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Como as feature-test macros funcionam
// ─────────────────────────────────────────────────────────────────────────────
void demo_como_funcionam() {
    fmt::print("\n── 1.1 Feature-test macros ──\n");

    fmt::print("      Cada feature define uma macro com a DATA de adoção:\n");
    fmt::print("        __cpp_lib_expected == 202211  → adotada em nov/2022\n");
    fmt::print("      O valor permite testar VERSÕES da mesma feature:\n");
    fmt::print("        #if __cpp_lib_optional >= 202110   // ops monádicas\n");

    fmt::print("\n      Onde elas moram:\n");
    fmt::print("        __cpp_*        → predefinidas pelo COMPILADOR (linguagem)\n");
    fmt::print("        __cpp_lib_*    → definidas em <version> (biblioteca)\n");
    fmt::print("      Incluir `<version>` é a forma barata de ter todas: ele não\n");
    fmt::print("      traz declaração nenhuma, só as macros.\n");

    fmt::print("\n      O padrão do C++ padroniza essas macros junto com a\n");
    fmt::print("      feature — não é convenção de compilador. É a única forma\n");
    fmt::print("      PORTÁVEL de escrever código que se adapta.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O inventário — gerado em compilação
// ─────────────────────────────────────────────────────────────────────────────

// Macro auxiliar: imprime o nome, se está presente, e a data de adoção
#define RELATAR(macro, descricao)                                              \
    do {                                                                        \
        fmt::print("      {:<34} ", descricao);                                 \
        _relatar_valor(macro);                                                  \
    } while (0)

#define RELATAR_AUSENTE(descricao)                                             \
    fmt::print("      {:<34} ✗ ausente\n", descricao)

inline void _relatar_valor(long v) { fmt::print("✓ {}\n", v); }

void demo_inventario_linguagem() {
    fmt::print("\n── 1.2 Linguagem: o que este GCC tem ──\n");

#ifdef __cpp_if_consteval
    RELATAR(__cpp_if_consteval, "if consteval");
#else
    RELATAR_AUSENTE("if consteval");
#endif

#ifdef __cpp_multidimensional_subscript
    RELATAR(__cpp_multidimensional_subscript, "operator[](i, j, k)");
#else
    RELATAR_AUSENTE("operator[](i, j, k)");
#endif

#ifdef __cpp_static_call_operator
    RELATAR(__cpp_static_call_operator, "static operator()");
#else
    RELATAR_AUSENTE("static operator()");
#endif

#ifdef __cpp_size_t_suffix
    RELATAR(__cpp_size_t_suffix, "sufixo uz / z");
#else
    RELATAR_AUSENTE("sufixo uz / z");
#endif

#ifdef __cpp_named_character_escapes
    RELATAR(__cpp_named_character_escapes, "\\N{NOME DO CARACTERE}");
#else
    RELATAR_AUSENTE("\\N{NOME DO CARACTERE}");
#endif

#ifdef __cpp_explicit_this_parameter
    RELATAR(__cpp_explicit_this_parameter, "deducing this");
#else
    RELATAR_AUSENTE("deducing this");
#endif

    fmt::print("\n      ↑ o `deducing this` (P0847) é a ausência mais sentida:\n");
    fmt::print("        ele resolveria a recursão em lambda e o CRTP sem\n");
    fmt::print("        template. Chegou no GCC 14.\n");
    fmt::print("        Ver src/lambdas/demo_padroes.hpp, 5.3.\n");
}

void demo_inventario_biblioteca() {
    fmt::print("\n── 1.3 Biblioteca: o que este libstdc++ tem ──\n");

    fmt::print("      PRESENTES:\n");
#ifdef __cpp_lib_expected
    RELATAR(__cpp_lib_expected, "std::expected");
#endif
#ifdef __cpp_lib_byteswap
    RELATAR(__cpp_lib_byteswap, "std::byteswap");
#endif
#ifdef __cpp_lib_to_underlying
    RELATAR(__cpp_lib_to_underlying, "std::to_underlying");
#endif
#ifdef __cpp_lib_unreachable
    RELATAR(__cpp_lib_unreachable, "std::unreachable");
#endif
#ifdef __cpp_lib_ranges_zip
    RELATAR(__cpp_lib_ranges_zip, "views::zip / zip_transform");
#endif
#ifdef __cpp_lib_string_contains
    RELATAR(__cpp_lib_string_contains, "string::contains");
#endif
#ifdef __cpp_lib_stacktrace
    RELATAR(__cpp_lib_stacktrace, "std::stacktrace*");
#endif

    fmt::print("\n      AUSENTES (precisam de GCC mais novo):\n");
#ifndef __cpp_lib_print
    RELATAR_AUSENTE("std::print / println");
#endif
#ifndef __cpp_lib_mdspan
    RELATAR_AUSENTE("std::mdspan");
#endif
#ifndef __cpp_lib_generator
    RELATAR_AUSENTE("std::generator");
#endif
#ifndef __cpp_lib_ranges_to_container
    RELATAR_AUSENTE("ranges::to<Container>()");
#endif
#ifndef __cpp_lib_flat_map
    RELATAR_AUSENTE("std::flat_map / flat_set");
#endif

    fmt::print("\n      ⚠  o `*` no `std::stacktrace`: a MACRO está definida e o\n");
    fmt::print("         header existe, mas o link falha sem uma biblioteca extra:\n");
    fmt::print("           undefined reference to `__glibcxx_backtrace_simple'\n");
    fmt::print("         É preciso `-lstdc++_libbacktrace`. A macro diz que a\n");
    fmt::print("         API existe, NÃO que ela linka com as flags padrão.\n");
    fmt::print("         Este subprojeto não linka a lib extra, então não usa.\n");
}

#undef RELATAR
#undef RELATAR_AUSENTE

// ─────────────────────────────────────────────────────────────────────────────
//  3. Degradar com elegância
//
//     O padrão que permite usar uma feature nova sem quebrar em toolchains
//     antigos: `#if` na macro, e um fallback equivalente.
// ─────────────────────────────────────────────────────────────────────────────
#include <bit>
#include <cstdint>

// Exemplo real: `std::byteswap` é C++23. O fallback usa o builtin do GCC.
constexpr std::uint32_t trocar_bytes(std::uint32_t v) {
#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L
    return std::byteswap(v);
#elif defined(__GNUC__)
    return __builtin_bswap32(v);
#else
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
#endif
}

void demo_degradar() {
    fmt::print("\n── 1.4 Degradar com elegância ──\n");

    static_assert(trocar_bytes(0x12345678u) == 0x78563412u);
    fmt::print("      trocar_bytes(0x12345678) = 0x{:08X} ✓\n",
               trocar_bytes(0x12345678u));

    fmt::print("\n      A mesma função, três implementações escolhidas em\n");
    fmt::print("      compilação:\n");
    fmt::print("        #if __cpp_lib_byteswap >= 202110L → std::byteswap\n");
    fmt::print("        #elif __GNUC__                    → __builtin_bswap32\n");
    fmt::print("        #else                             → shifts portáveis\n");

    fmt::print("\n      As três regras deste padrão:\n");
    fmt::print("        1. teste a MACRO, não a versão do compilador\n");
    fmt::print("           (`#if __GNUC__ >= 13` erra em Clang e MSVC)\n");
    fmt::print("        2. compare com o VALOR, não só `#ifdef`\n");
    fmt::print("           (features ganham versões novas)\n");
    fmt::print("        3. o fallback precisa ser EQUIVALENTE, não aproximado\n");
    fmt::print("           — o `static_assert` acima verifica isso\n");

    fmt::print("\n      Este repositório usa o padrão em vários lugares:\n");
    fmt::print("        src/bits/demo_endianness.hpp    (byteswap)\n");
    fmt::print("        src/chrono/demo_calendario.hpp  (fusos horários)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Onde consultar
// ─────────────────────────────────────────────────────────────────────────────
void demo_referencias() {
    fmt::print("\n── 1.5 Onde consultar ──\n");

    fmt::print("      Para saber se uma feature está disponível:\n");
    fmt::print("        1. cppreference.com/w/cpp/feature_test — a lista completa\n");
    fmt::print("           de macros, com a versão de cada compilador\n");
    fmt::print("        2. gcc.gnu.org/projects/cxx-status.html — status do GCC\n");
    fmt::print("        3. libcxx.llvm.org/Status/Cxx23.html   — status do libc++\n");
    fmt::print("        4. este demo, que MEDE em vez de consultar\n");

    fmt::print("\n      E para reproduzir a medição num toolchain qualquer:\n");
    fmt::print("        echo '#include <version>' > /tmp/v.cpp\n");
    fmt::print("        g++ -std=c++23 -dM -E /tmp/v.cpp | grep __cpp_lib | sort\n");
    fmt::print("      → despeja TODAS as macros de biblioteca definidas.\n");

    fmt::print("\n      A lição que este demo existe para ensinar:\n");
    fmt::print("      NÃO PRESUMA. A tabela do src/ROADMAP.md deste repositório\n");
    fmt::print("      tinha DUAS linhas erradas (`stacktrace` e `views::zip`\n");
    fmt::print("      listados como ausentes) até serem medidos com o código\n");
    fmt::print("      acima. Documentação de terceiros envelhece; a macro não.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_como_funcionam();
    demo_inventario_linguagem();
    demo_inventario_biblioteca();
    demo_degradar();
    demo_referencias();
}

} // namespace demo_inventario
