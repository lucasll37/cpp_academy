// =============================================================================
//  src/undefined_behavior/demo_aliasing.hpp
//
//  Strict aliasing — o UB sem rede de proteção
//  ───────────────────────────────────────────
//  A regra (§[basic.lval]): acessar o valor de um objeto por um lvalue de
//  tipo INCOMPATÍVEL é UB. As exceções são poucas e valem decorar:
//
//    • o tipo exato do objeto (com ou sem cv);
//    • um tipo com sinal / sem sinal correspondente (int ↔ unsigned int);
//    • uma classe base;
//    • `char`, `unsigned char` e `std::byte` — os três podem ver QUALQUER
//      objeto como sequência de bytes.
//
//  Ela existe para o otimizador: se um `float*` não pode apontar para um
//  `int`, escrever no float não invalida o valor do int guardado em
//  registrador. Sem a regra, toda escrita por ponteiro forçaria recarregar
//  tudo.
//
//  ⚠ Nenhum sanitizer pega violação de strict aliasing (medido em 3.2).
//
//  Tópicos:
//  • A regra e as exceções
//  • O type punning errado e o certo
//  • std::bit_cast, e por que ele resolve
//  • ⚠ union: legal em C, UB em C++
//  • -fno-strict-aliasing e o que ele custa
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <bit>
#include <cstdint>
#include <cstring>

namespace demo_aliasing {

void demo_regra() {
    fmt::print("\n── 4.1 A regra ──\n");

    fmt::print("      Por que ela existe — considere:\n\n");
    fmt::print("        void f(int* pi, float* pf) {{\n");
    fmt::print("            *pi = 1;\n");
    fmt::print("            *pf = 2.0f;\n");
    fmt::print("            printf(\"%d\", *pi);      // pode reusar o 1 do registrador?\n");
    fmt::print("        }}\n\n");
    fmt::print("      Com strict aliasing, SIM: `pf` não pode apontar para o mesmo\n");
    fmt::print("      objeto que `pi`, logo `*pi` continua sendo 1 e a releitura é\n");
    fmt::print("      desnecessária. Sem a regra, toda escrita por ponteiro\n");
    fmt::print("      invalidaria todos os valores em registrador.\n\n");

    fmt::print("      As exceções — tipos que PODEM ver um objeto:\n");
    fmt::print("        • o tipo exato (com ou sem const/volatile);\n");
    fmt::print("        • signed ↔ unsigned do mesmo tipo;\n");
    fmt::print("        • uma classe BASE do tipo real;\n");
    fmt::print("        • char, unsigned char, std::byte  ← os únicos universais\n\n");

    fmt::print("      ⚠  `signed char` NÃO está na lista dos universais. São só\n");
    fmt::print("         `char`, `unsigned char` e `std::byte`.\n");
}

void demo_punning() {
    fmt::print("\n── 4.2 Type punning: o errado e o certo ──\n");

    const float f = 3.14f;

    fmt::print("      ❌ ERRADO — viola strict aliasing:\n");
    fmt::print("        int i = *reinterpret_cast<const int*>(&f);\n");
    fmt::print("      Compila, roda, dá o resultado \"certo\" na maioria dos\n");
    fmt::print("      casos, e é UB. Nenhum sanitizer avisa. O GCC tem\n");
    fmt::print("      -Wstrict-aliasing, mas ele só pega casos simples e dá\n");
    fmt::print("      falsos negativos com frequência.\n\n");

    // ✓ Correto (1): memcpy — sempre funcionou, e o compilador o elimina
    std::uint32_t via_memcpy = 0;
    std::memcpy(&via_memcpy, &f, sizeof f);

    // ✓ Correto (2): bit_cast — C++20, constexpr, sem ponteiro nenhum
    const auto via_bitcast = std::bit_cast<std::uint32_t>(f);

    fmt::print("      ✓ CERTO (1) — std::memcpy:\n");
    fmt::print("          std::memcpy(&destino, &origem, sizeof origem);\n");
    fmt::print("          → 0x{:08X}\n", via_memcpy);
    fmt::print("          Legal desde sempre. Não é lento: com -O2 o compilador\n");
    fmt::print("          reconhece o padrão e emite UM `mov`.\n\n");

    fmt::print("      ✓ CERTO (2) — std::bit_cast (C++20):\n");
    fmt::print("          auto i = std::bit_cast<std::uint32_t>(f);\n");
    fmt::print("          → 0x{:08X}\n", via_bitcast);
    fmt::print("          Idêntico? {}\n", via_memcpy == via_bitcast);

    fmt::print("\n      `bit_cast` é melhor que `memcpy` em quatro pontos:\n");
    fmt::print("        • exige `sizeof` IGUAL — erro de compilação se não for;\n");
    fmt::print("        • exige os dois tipos trivially copyable — idem;\n");
    fmt::print("        • não menciona ponteiro, então não há como errar o sizeof;\n");
    fmt::print("        • é `constexpr`.\n");

    // A prova do constexpr
    {
        constexpr auto bits = std::bit_cast<std::uint32_t>(1.0f);
        static_assert(bits == 0x3F800000u, "IEEE-754: 1.0f");
        fmt::print("\n        static_assert(bit_cast<uint32_t>(1.0f) == 0x3F800000)\n");
        fmt::print("        → passou em COMPILAÇÃO. Ver src/bits/demo_bit.hpp.\n");
    }
}

void demo_union() {
    fmt::print("\n── 4.3 ⚠ union: legal em C, UB em C++ ──\n");

    fmt::print("        union U {{ float f; std::uint32_t i; }};\n");
    fmt::print("        U u; u.f = 3.14f;\n");
    fmt::print("        std::uint32_t bits = u.i;      // ← ler o membro INATIVO\n\n");

    fmt::print("      Em C isto é EXPLICITAMENTE permitido desde o C99 (é o\n");
    fmt::print("      idioma padrão de type punning na linguagem).\n");
    fmt::print("      Em C++ é UB: ler o membro que não é o ATIVO viola a regra\n");
    fmt::print("      de tempo de vida — só um membro do union existe por vez.\n\n");

    fmt::print("      Na prática, GCC e Clang aceitam e documentam como extensão.\n");
    fmt::print("      Mas o código deixa de ser portável, e o MSVC não dá a mesma\n");
    fmt::print("      garantia. Use `bit_cast`.\n\n");

    fmt::print("      Onde union AINDA faz sentido em C++:\n");
    fmt::print("        • interoperar com uma API de C que usa union;\n");
    fmt::print("        • implementar um tipo-soma à mão — e aí `std::variant`\n");
    fmt::print("          já faz melhor (src/vocabulary_types/demo_variant.hpp);\n");
    fmt::print("        • armazenamento com tempo de vida manual, com\n");
    fmt::print("          placement new e destrutor explícito.\n");
}

void demo_flags() {
    fmt::print("\n── 4.4 -fno-strict-aliasing ──\n");

    fmt::print("      A saída de emergência: desligar a otimização baseada na\n");
    fmt::print("      regra. O compilador passa a assumir que QUALQUER ponteiro\n");
    fmt::print("      pode apontar para qualquer coisa.\n\n");

    fmt::print("      Quem usa: o kernel Linux compila com essa flag desde 2003,\n");
    fmt::print("      depois de uma série de bugs. Não porque a regra seja ruim,\n");
    fmt::print("      mas porque auditar milhões de linhas de C antigo é inviável.\n\n");

    fmt::print("      O custo é real mas específico: afeta laços que escrevem por\n");
    fmt::print("      um ponteiro e leem por outro. Num código C++ moderno, que\n");
    fmt::print("      usa containers e referências em vez de aritmética de\n");
    fmt::print("      ponteiro, o impacto tende a ser pequeno.\n\n");

    fmt::print("      Ordem de preferência:\n");
    fmt::print("        1. `std::bit_cast` ou `memcpy` — corrige o problema;\n");
    fmt::print("        2. `char`/`std::byte` quando você precisa MESMO ver bytes;\n");
    fmt::print("        3. `__attribute__((may_alias))` num tipo específico\n");
    fmt::print("           (GCC/Clang) — cirúrgico, mas não portável;\n");
    fmt::print("        4. -fno-strict-aliasing no projeto inteiro — o martelo.\n\n");

    fmt::print("      ⚠  Se você usa a flag, ela precisa valer para TODAS as\n");
    fmt::print("         unidades de tradução. Uma TU compilada sem ela pode\n");
    fmt::print("         otimizar em cima de uma suposição que outra viola — e\n");
    fmt::print("         isso é a violação de ODR silenciosa de src/linkage/ 2.4.\n");
}

inline void run() {
    demo_regra();
    demo_punning();
    demo_union();
    demo_flags();
}

}  // namespace demo_aliasing
