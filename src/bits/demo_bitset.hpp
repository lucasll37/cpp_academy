// =============================================================================
//  src/bits/demo_bitset.hpp
//
//  std::bitset — um conjunto de bits com nome
//  ──────────────────────────────────────────
//  `bitset<N>` é um array de N bits empacotados, com uma interface de alto
//  nível. Ele resolve o problema de legibilidade das máscaras cruas: em vez de
//  `flags & (1u << 3)`, você escreve `flags.test(3)`.
//
//  A limitação que decide o uso: N é fixo em COMPILAÇÃO. Se o tamanho varia
//  em runtime, `bitset` não serve — e a alternativa da STL (`vector<bool>`)
//  tem os problemas de src/stl/demo_pitfalls.hpp, 5.2.
//
//  Tópicos:
//  • A interface, e o que ela ganha sobre máscaras cruas
//  • Conversão de e para inteiro e string
//  • Operações de conjunto
//  • bitset vs vector<bool> vs máscara crua vs enum de flags
//  • ⚠ A ordem dos índices vs a ordem da string
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace demo_bitset {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A interface
// ─────────────────────────────────────────────────────────────────────────────
void demo_interface() {
    fmt::print("\n── 3.1 A interface ──\n");

    std::bitset<8> b{0b0000'1010};

    fmt::print("      bitset<8>{{0b00001010}} = {}\n", b.to_string());
    fmt::print("        size()   = {}\n", b.size());
    fmt::print("        count()  = {}   bits ligados\n", b.count());
    fmt::print("        any()    = {}\n", b.any());
    fmt::print("        all()    = {}\n", b.all());
    fmt::print("        none()   = {}\n", b.none());
    fmt::print("        test(1)  = {}\n", b.test(1));
    fmt::print("        test(2)  = {}\n", b.test(2));

    b.set(0);              fmt::print("\n      set(0)     → {}\n", b.to_string());
    b.reset(1);            fmt::print("      reset(1)   → {}\n", b.to_string());
    b.flip(7);             fmt::print("      flip(7)    → {}\n", b.to_string());
    b[2] = true;           fmt::print("      b[2]=true  → {}\n", b.to_string());

    fmt::print("\n      Compare com a versão de máscara crua:\n");
    fmt::print("        b.test(3)          vs   (flags >> 3) & 1\n");
    fmt::print("        b.set(3)           vs   flags |= (1u << 3)\n");
    fmt::print("        b.reset(3)         vs   flags &= ~(1u << 3)\n");
    fmt::print("        b.count()          vs   __builtin_popcount(flags)\n");
    fmt::print("      A versão com bitset é mais legível e igualmente rápida —\n");
    fmt::print("      as operações compilam para as mesmas instruções.\n");

    fmt::print("\n      ⚠  `test(i)` verifica os limites e lança out_of_range;\n");
    fmt::print("         `b[i]` NÃO verifica — é UB fora da faixa, igual ao\n");
    fmt::print("         `vector::at` vs `vector::operator[]`.\n");

    try {
        (void)b.test(100);
    } catch (const std::out_of_range& e) {
        fmt::print("         b.test(100) → out_of_range: {}\n", e.what());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ A ordem dos índices vs a ordem da string
//
//     O índice 0 é o bit MENOS significativo — o mais à DIREITA na string.
//     Isso confunde na primeira vez e é fonte de bug ao converter.
// ─────────────────────────────────────────────────────────────────────────────
void demo_ordem() {
    fmt::print("\n── 3.2 ⚠ A ordem dos índices ──\n");

    std::bitset<8> b;
    b.set(0);          // bit MENOS significativo

    fmt::print("      bitset<8> b; b.set(0);\n");
    fmt::print("        to_string() = {}\n", b.to_string());
    fmt::print("        to_ulong()  = {}\n", b.to_ulong());
    fmt::print("      ↑ o índice 0 apareceu na DIREITA da string.\n");

    b.reset();
    b.set(7);
    fmt::print("\n      b.set(7):\n");
    fmt::print("        to_string() = {}\n", b.to_string());
    fmt::print("        to_ulong()  = {}\n", b.to_ulong());

    fmt::print("\n      A regra:\n");
    fmt::print("        índice i    → valor 2^i\n");
    fmt::print("        string[j]   → índice (N-1-j)\n");
    fmt::print("      Ou seja: a STRING lê da esquerda para a direita como um\n");
    fmt::print("      número binário; os ÍNDICES contam da direita para a\n");
    fmt::print("      esquerda, como potências.\n");

    // Construir a partir de string: o mesmo cuidado
    const std::bitset<8> de_string{"00000011"};
    fmt::print("\n      bitset<8>{{\"00000011\"}}:\n");
    fmt::print("        to_ulong() = {} | test(0)={} test(1)={} test(7)={}\n",
               de_string.to_ulong(), de_string.test(0),
               de_string.test(1), de_string.test(7));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Conversões e operações de conjunto
// ─────────────────────────────────────────────────────────────────────────────
void demo_conjuntos() {
    fmt::print("\n── 3.3 Operações de conjunto ──\n");

    // Um bitset como conjunto de permissões
    const std::bitset<8> leitura {0b0000'0001};
    const std::bitset<8> escrita {0b0000'0010};
    const std::bitset<8> execucao{0b0000'0100};

    const auto usuario = leitura | escrita;
    const auto grupo   = leitura | execucao;

    fmt::print("      usuário  : {} (leitura|escrita)\n",  usuario.to_string());
    fmt::print("      grupo    : {} (leitura|execução)\n", grupo.to_string());

    fmt::print("\n      união        (|) : {}\n", (usuario | grupo).to_string());
    fmt::print("      interseção   (&) : {}\n", (usuario & grupo).to_string());
    fmt::print("      diferença simétrica (^) : {}\n", (usuario ^ grupo).to_string());
    fmt::print("      complemento  (~) : {}\n", (~usuario).to_string());
    fmt::print("      diferença (a & ~b): {}\n", (usuario & ~grupo).to_string());

    fmt::print("\n      contém tudo de `leitura`? {}\n",
               (usuario & leitura) == leitura);
    fmt::print("      são disjuntos?           {}\n", (usuario & grupo).none());

    // Deslocamento também funciona
    const std::bitset<8> v{0b0000'0011};
    fmt::print("\n      {} << 2 = {}\n", v.to_string(), (v << 2).to_string());
    fmt::print("      {} >> 1 = {}\n", v.to_string(), (v >> 1).to_string());

    // Conversões numéricas
    fmt::print("\n      conversões:\n");
    fmt::print("        to_ulong()  → {}\n",  v.to_ulong());
    fmt::print("        to_ullong() → {}\n",  v.to_ullong());
    fmt::print("        to_string() → '{}'\n", v.to_string());
    fmt::print("      ⚠  `to_ulong()` lança `overflow_error` se o bitset tiver\n");
    fmt::print("         mais bits significativos do que cabe num unsigned long.\n");

    const std::bitset<100> grande;
    fmt::print("        bitset<100> vazio → to_ulong() = {} (ok, é zero)\n",
               grande.to_ulong());
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. bitset vs as alternativas
// ─────────────────────────────────────────────────────────────────────────────
enum class Permissao : std::uint8_t {
    Nenhuma  = 0,
    Leitura  = 1u << 0,
    Escrita  = 1u << 1,
    Execucao = 1u << 2,
};

constexpr Permissao operator|(Permissao a, Permissao b) {
    return static_cast<Permissao>(static_cast<std::uint8_t>(a) |
                                  static_cast<std::uint8_t>(b));
}
constexpr bool contem(Permissao conjunto, Permissao item) {
    return (static_cast<std::uint8_t>(conjunto) &
            static_cast<std::uint8_t>(item)) != 0;
}

void demo_alternativas() {
    fmt::print("\n── 3.4 bitset vs as alternativas ──\n");

    fmt::print("      | opção            | tamanho | legível | seguro | dinâmico|\n");
    fmt::print("      |------------------|---------|---------|--------|---------|\n");
    fmt::print("      | uint32_t + máscara| fixo   | não     | não    | não     |\n");
    fmt::print("      | enum class flags | fixo    | SIM     | SIM    | não     |\n");
    fmt::print("      | std::bitset<N>   | fixo    | SIM     | sim    | não     |\n");
    fmt::print("      | vector<bool>     | DINÂM.  | sim     | não*   | SIM     |\n");
    fmt::print("      | vector<char>     | dinâm.  | sim     | sim    | sim     |\n");
    fmt::print("      * vector<bool> devolve proxy — ver src/stl/demo_pitfalls, 5.2\n");

    // O enum de flags, que é o mais type-safe para conjuntos pequenos e nomeados
    const auto p = Permissao::Leitura | Permissao::Escrita;
    fmt::print("\n      enum class como conjunto de flags:\n");
    fmt::print("        Leitura|Escrita contém Leitura?  {}\n", contem(p, Permissao::Leitura));
    fmt::print("        Leitura|Escrita contém Execução? {}\n", contem(p, Permissao::Execucao));
    fmt::print("      ↑ o `enum class` impede misturar com outro enum ou com int.\n");
    fmt::print("        Exige escrever os operadores, mas cada flag tem NOME.\n");

    fmt::print("\n      A escolha:\n");
    fmt::print("        poucas flags, NOMEADAS      → enum class + operadores\n");
    fmt::print("        muitos bits, tamanho fixo   → std::bitset<N>\n");
    fmt::print("        tamanho só conhecido em run → vector<char> ou\n");
    fmt::print("                                      boost::dynamic_bitset\n");
    fmt::print("        interoperar com API C/binário → uint32_t + máscara\n");

    fmt::print("\n      sizeof: bitset<8>={} bitset<64>={} bitset<65>={} bitset<100>={}\n",
               sizeof(std::bitset<8>), sizeof(std::bitset<64>),
               sizeof(std::bitset<65>), sizeof(std::bitset<100>));
    fmt::print("      ↑ o bitset arredonda para múltiplos da palavra (8 bytes).\n");
    fmt::print("        `bitset<65>` já custa 16 bytes.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Um uso real: crivo de Eratóstenes
// ─────────────────────────────────────────────────────────────────────────────
void demo_uso_real() {
    fmt::print("\n── 3.5 Um uso real: crivo de Eratóstenes ──\n");

    constexpr std::size_t N = 1000;
    std::bitset<N + 1> composto;      // 1001 bits = ~126 bytes

    composto.set(0);
    composto.set(1);
    for (std::size_t i = 2; i * i <= N; ++i) {
        if (composto.test(i)) continue;
        for (std::size_t j = i * i; j <= N; j += i) composto.set(j);
    }

    const auto quantos_primos = (N + 1) - composto.count();
    fmt::print("      primos até {}: {}\n", N, quantos_primos);

    fmt::print("      os 15 primeiros: ");
    std::size_t impressos = 0;
    for (std::size_t i = 2; i <= N && impressos < 15; ++i) {
        if (!composto.test(i)) { fmt::print("{} ", i); ++impressos; }
    }
    fmt::print("\n");

    fmt::print("\n      memória usada:\n");
    fmt::print("        bitset<1001>      = {:>5} bytes\n", sizeof(composto));
    fmt::print("        vector<bool>(1001)= {:>5} bytes (aprox., + cabeçalho)\n", 1001 / 8);
    fmt::print("        vector<char>(1001)= {:>5} bytes\n", 1001);
    fmt::print("        bool[1001]        = {:>5} bytes\n", 1001);
    fmt::print("      ↑ 8× menos memória que um array de bool, e cabe todo no\n");
    fmt::print("        cache L1 — o que importa mais que o tamanho em si.\n");

    fmt::print("\n      O mesmo crivo aparece em src/compile_time/demo_constexpr.hpp,\n");
    fmt::print("      rodando em tempo de COMPILAÇÃO com `vector<bool>`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_interface();
    demo_ordem();
    demo_conjuntos();
    demo_alternativas();
    demo_uso_real();
}

} // namespace demo_bitset
