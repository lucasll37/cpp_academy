// =============================================================================
//  src/bits/demo_bit.hpp
//
//  <bit> — as operações que o hardware já sabia fazer
//  ──────────────────────────────────────────────────
//  Todo processador moderno tem instruções para contar bits, achar o bit mais
//  significativo e rotacionar. Até o C++20 não havia forma PORTÁVEL de chamá-las:
//  o código usava `__builtin_popcount` (GCC), `_BitScanReverse` (MSVC), ou um
//  laço que o compilador talvez reconhecesse.
//
//  O `<bit>` do C++20 padronizou tudo — e, de quebra, resolveu o problema mais
//  antigo do type punning com `std::bit_cast`.
//
//  Tópicos:
//  • bit_cast — o fim do type punning por união e reinterpret_cast
//  • Contagem: popcount, countl_zero, countr_zero
//  • Potências de dois: has_single_bit, bit_ceil, bit_floor, bit_width
//  • Rotação: rotl, rotr
//  • O código gerado — verificando que vira UMA instrução
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <bit>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace demo_bit {

template <typename T>
std::string bin(T v) {
    return std::bitset<sizeof(T) * 8>{static_cast<unsigned long long>(v)}.to_string();
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::bit_cast — o fim do type punning informal
//
//     Reinterpretar os bits de um tipo como outro é operação legítima e comum
//     (serialização, algoritmos de ponto flutuante). Todas as formas antigas
//     eram UB ou verbosas:
//
//       *(int*)&f                 ← ✗ viola strict aliasing: UB
//       union { float f; int i; } ← ✗ ler membro inativo é UB em C++ (ok em C)
//       memcpy(&i, &f, 4)         ← ✓ correto, mas verboso e não é constexpr
//       std::bit_cast<int>(f)     ← ✓ correto, conciso, E constexpr
// ─────────────────────────────────────────────────────────────────────────────
void demo_bit_cast() {
    fmt::print("\n── 2.1 std::bit_cast ──\n");

    const float f = 1.0f;
    const auto bits = std::bit_cast<std::uint32_t>(f);

    fmt::print("      float 1.0f em bits: {} (0x{:08X})\n", bin(bits), bits);
    fmt::print("        sinal    [31]    : {}\n", bits >> 31);
    fmt::print("        expoente [30..23]: {} (viés 127 → expoente real {})\n",
               (bits >> 23) & 0xFF, static_cast<int>((bits >> 23) & 0xFF) - 127);
    fmt::print("        mantissa [22.. 0]: 0x{:06X}\n", bits & 0x7FFFFF);

    // E o caminho de volta
    const auto de_volta = std::bit_cast<float>(bits);
    fmt::print("      bit_cast de volta  : {} (round-trip exato: {})\n",
               de_volta, de_volta == f);

    // A grande vantagem sobre memcpy: funciona em contexto constexpr
    constexpr auto bits_const = std::bit_cast<std::uint32_t>(2.0f);
    static_assert(bits_const == 0x40000000u);
    fmt::print("\n      constexpr auto b = bit_cast<uint32_t>(2.0f);\n");
    fmt::print("        → 0x{:08X}, verificado por static_assert ✓\n", bits_const);
    fmt::print("      `memcpy` NÃO é constexpr — o bit_cast é.\n");

    // Os requisitos
    fmt::print("\n      Os requisitos de `bit_cast<To>(from)`:\n");
    fmt::print("        • sizeof(To) == sizeof(From)\n");
    fmt::print("        • ambos trivialmente copiáveis\n");
    fmt::print("      Verificados em COMPILAÇÃO — não há como errar em runtime.\n");

    // Um uso real: comparar floats pela representação, para ordenação total
    const float nan1 = std::bit_cast<float>(0x7FC00000u);
    fmt::print("\n      uso real — inspecionar um NaN:\n");
    fmt::print("        bits 0x7FC00000 → {} (é NaN: {})\n", nan1, nan1 != nan1);
    fmt::print("        `nan != nan` é a única forma portátil de detectar NaN\n");
    fmt::print("        com comparação — e o bit_cast mostra POR QUÊ.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Contagem de bits
// ─────────────────────────────────────────────────────────────────────────────
void demo_contagem() {
    fmt::print("\n── 2.2 Contagem de bits ──\n");

    const std::uint32_t v = 0b0000'1011'0110'0000'0000'0000'0000'0000u;

    fmt::print("      v = {}\n", bin(v));
    fmt::print("        popcount(v)    = {}   bits 1 no total\n",   std::popcount(v));
    fmt::print("        countl_zero(v) = {}   zeros à ESQUERDA\n",  std::countl_zero(v));
    fmt::print("        countl_one(v)  = {}   uns à esquerda\n",    std::countl_one(v));
    fmt::print("        countr_zero(v) = {}  zeros à DIREITA\n",    std::countr_zero(v));
    fmt::print("        countr_one(v)  = {}   uns à direita\n",     std::countr_one(v));

    fmt::print("\n      Os usos práticos:\n");
    fmt::print("        popcount    → distância de Hamming, cardinalidade de\n");
    fmt::print("                      conjunto em bitmask, paridade\n");
    fmt::print("        countl_zero → posição do bit mais significativo → log2\n");
    fmt::print("        countr_zero → posição do bit menos significativo →\n");
    fmt::print("                      alinhamento de um ponteiro, próximo item\n");
    fmt::print("                      livre num allocator de bitmap\n");

    // Exemplo: distância de Hamming entre dois hashes
    const std::uint64_t h1 = 0xDEADBEEFCAFEBABEull;
    const std::uint64_t h2 = 0xDEADBEEFCAFEBABFull;
    fmt::print("\n      distância de Hamming entre dois hashes de 64 bits:\n");
    fmt::print("        popcount(h1 ^ h2) = {} bit(s) diferentes\n",
               std::popcount(h1 ^ h2));

    // Exemplo: alinhamento de um endereço
    alignas(64) int buffer[16]{};
    const auto endereco = std::bit_cast<std::uintptr_t>(&buffer[0]);
    fmt::print("\n      alinhamento de um buffer alignas(64):\n");
    fmt::print("        endereço termina em {} zeros → alinhado a {} bytes\n",
               std::countr_zero(endereco), 1ull << std::countr_zero(endereco));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Potências de dois
// ─────────────────────────────────────────────────────────────────────────────
void demo_potencias() {
    fmt::print("\n── 2.3 Potências de dois ──\n");

    fmt::print("      | valor | single_bit | bit_width | bit_floor | bit_ceil |\n");
    fmt::print("      |-------|------------|-----------|-----------|----------|\n");
    for (std::uint32_t v : {0u, 1u, 2u, 3u, 7u, 8u, 100u, 1000u}) {
        fmt::print("      | {:>5} | {:<10} | {:>9} | {:>9} | {:>8} |\n",
                   v,
                   std::has_single_bit(v) ? "sim" : "não",
                   std::bit_width(v),
                   std::bit_floor(v),
                   std::bit_ceil(v));
    }

    fmt::print("\n      O que cada uma responde:\n");
    fmt::print("        has_single_bit → \"é potência de 2?\"\n");
    fmt::print("        bit_width      → quantos bits são necessários (⌊log2⌋+1)\n");
    fmt::print("        bit_floor      → maior potência de 2 <= v\n");
    fmt::print("        bit_ceil       → menor potência de 2 >= v\n");

    fmt::print("\n      O uso canônico é dimensionar um buffer:\n");
    const std::uint32_t solicitado = 1000;
    fmt::print("        pedido {} elementos → alocar bit_ceil = {}\n",
               solicitado, std::bit_ceil(solicitado));
    fmt::print("        (potência de 2 permite `i & (n-1)` em vez de `i % n`)\n");

    fmt::print("\n      Antes do C++20, `has_single_bit(v)` era escrito como\n");
    fmt::print("        v && !(v & (v - 1))\n");
    fmt::print("      — correto, mas ilegível, e errado para v == 0 se você\n");
    fmt::print("      esquecesse o primeiro termo.\n");

    // ⚠ bit_ceil tem um caso de UB
    fmt::print("\n      ⚠  `bit_ceil(v)` é UB se o resultado não couber no tipo:\n");
    fmt::print("         bit_ceil(uint32_t(0x80000001)) → o próximo é 2^32 → UB\n");
    fmt::print("         Verifique antes se v <= bit_floor(max/2).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Rotação
//
//     Rotacionar é diferente de deslocar: os bits que saem de um lado ENTRAM
//     no outro. É a operação-base de criptografia e de funções de hash.
// ─────────────────────────────────────────────────────────────────────────────
void demo_rotacao() {
    fmt::print("\n── 2.4 Rotação ──\n");

    const std::uint8_t v = 0b1001'0011;

    fmt::print("      v          = {}\n", bin(v));
    fmt::print("      v << 3     = {}  (shift: entram zeros, perde bits)\n",
               bin(static_cast<std::uint8_t>(v << 3)));
    fmt::print("      rotl(v, 3) = {}  (rotação: os bits VOLTAM pelo outro lado)\n",
               bin(std::rotl(v, 3)));
    fmt::print("      rotr(v, 3) = {}\n", bin(std::rotr(v, 3)));

    // A propriedade que define: rotacionar N vezes por 1 = rotacionar por N
    const auto ida  = std::rotl(v, 5);
    const auto volta = std::rotr(ida, 5);
    fmt::print("\n      rotr(rotl(v,5),5) == v ? {}  ← rotação é reversível\n", volta == v);

    // rotl aceita contagem NEGATIVA e maior que a largura, sem UB
    fmt::print("\n      diferente do shift, a rotação NÃO tem UB:\n");
    fmt::print("        rotl(v,  11) = {}  (11 % 8 = 3)\n", bin(std::rotl(v, 11)));
    fmt::print("        rotl(v,  -3) = {}  (negativo = rotr)\n", bin(std::rotl(v, -3)));
    fmt::print("      ↑ a contagem é reduzida módulo a largura, e negativo\n");
    fmt::print("        inverte o sentido. É definido para QUALQUER valor.\n");

    fmt::print("\n      Antes do C++20, escrever rotação era uma armadilha:\n");
    fmt::print("        (v << n) | (v >> (32 - n))       // ✗ UB quando n == 0\n");
    fmt::print("        (v << n) | (v >> ((32 - n) % 32)) // ✓ mas ilegível\n");
    fmt::print("      E o compilador só reconhecia o idioma EXATO para emitir a\n");
    fmt::print("      instrução ROL. Com `std::rotl`, é garantido.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Verificando que vira uma instrução só
// ─────────────────────────────────────────────────────────────────────────────
void demo_codigo_gerado() {
    fmt::print("\n── 2.5 O código gerado ──\n");

    fmt::print("      Estas funções mapeiam para UMA instrução em x86-64:\n");
    fmt::print("        std::popcount     → POPCNT   (SSE4.2)\n");
    fmt::print("        std::countl_zero  → LZCNT    (BMI1) ou BSR\n");
    fmt::print("        std::countr_zero  → TZCNT    (BMI1) ou BSF\n");
    fmt::print("        std::rotl/rotr    → ROL/ROR\n");
    fmt::print("        std::bit_cast     → nada (é só reinterpretar registrador)\n");

    fmt::print("\n      ⚠  mas SÓ se o compilador puder usar a extensão:\n");
    fmt::print("        g++ -O2               → POPCNT não é garantido\n");
    fmt::print("        g++ -O2 -msse4.2      → POPCNT ✓\n");
    fmt::print("        g++ -O2 -march=native → tudo que a sua CPU tiver\n");
    fmt::print("      Sem a flag, o compilador emite uma sequência genérica de\n");
    fmt::print("      ~12 instruções (o algoritmo de Hamming weight).\n");

    fmt::print("\n      Confira você mesmo:\n");
    fmt::print("        echo 'int f(unsigned x){{return __builtin_popcount(x);}}' \\\n");
    fmt::print("          | g++ -O2 -msse4.2 -S -x c++ - -o - | grep -i popcnt\n");

    fmt::print("\n      Este subprojeto compila sem `-msse4.2` de propósito: o\n");
    fmt::print("      objetivo é o COMPORTAMENTO, não a instrução. Para ver o\n");
    fmt::print("      lado do assembly, ver src/asm_course/09_simd_avx e\n");
    fmt::print("      src/inline_asm/.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_bit_cast();
    demo_contagem();
    demo_potencias();
    demo_rotacao();
    demo_codigo_gerado();
}

} // namespace demo_bit
