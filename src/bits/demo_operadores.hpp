// =============================================================================
//  src/bits/demo_operadores.hpp
//
//  Operadores de bit — e as regras que quase ninguém sabe
//  ──────────────────────────────────────────────────────
//  `&`, `|`, `^`, `~`, `<<`, `>>` parecem os operadores mais simples da
//  linguagem. Eles escondem três regras que produzem undefined behavior:
//
//    1. PROMOÇÃO INTEIRA — operandos menores que `int` viram `int` ANTES da
//       operação. `~uint8_t(0)` não é 0, é -1.
//    2. SHIFT >= largura do tipo é UB. `x << 32` num int de 32 bits é UB.
//    3. SHIFT à esquerda que estoura o SINAL é UB (até o C++20).
//
//  Tópicos:
//  • As máscaras: testar, ligar, desligar, alternar
//  • ⚠ Promoção inteira — o UB que o `~` esconde
//  • ⚠ Shift fora de faixa
//  • Aritmético vs lógico: `>>` em signed vs unsigned
//  • Extrair e inserir campos
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <bitset>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

namespace demo_operadores {

// Helper: imprime um valor em binário, com largura fixa
template <typename T>
std::string bin(T v) {
    return std::bitset<sizeof(T) * 8>{static_cast<unsigned long long>(v)}.to_string();
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. As quatro operações com máscara
// ─────────────────────────────────────────────────────────────────────────────
void demo_mascaras() {
    fmt::print("\n── 1.1 As quatro operações com máscara ──\n");

    std::uint8_t flags = 0b0000'1010;
    const std::uint8_t BIT_3 = 1u << 3;      // 0b0000'1000

    fmt::print("      flags inicial     : {}  (0x{:02X})\n", bin(flags), flags);
    fmt::print("      máscara BIT_3     : {}\n", bin(BIT_3));

    fmt::print("\n      TESTAR   (flags & BIT_3) != 0  → {}\n", (flags & BIT_3) != 0);

    flags = static_cast<std::uint8_t>(flags | BIT_3);
    fmt::print("      LIGAR    flags |= BIT_3        → {}\n", bin(flags));

    flags = static_cast<std::uint8_t>(flags & static_cast<std::uint8_t>(~BIT_3));
    fmt::print("      DESLIGAR flags &= ~BIT_3       → {}\n", bin(flags));

    flags = static_cast<std::uint8_t>(flags ^ BIT_3);
    fmt::print("      ALTERNAR flags ^= BIT_3        → {}\n", bin(flags));

    fmt::print("\n      | operação | idioma            |\n");
    fmt::print("      |----------|-------------------|\n");
    fmt::print("      | testar   | (x & M) != 0      |\n");
    fmt::print("      | ligar    | x |= M            |\n");
    fmt::print("      | desligar | x &= ~M           |\n");
    fmt::print("      | alternar | x ^= M            |\n");

    fmt::print("\n      ⚠  repare nos `static_cast<uint8_t>` acima. Sem eles o\n");
    fmt::print("         GCC avisa com -Wconversion, e o motivo é a promoção\n");
    fmt::print("         inteira — a próxima seção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ Promoção inteira — o UB que o `~` esconde
//
//     Todo operando menor que `int` é PROMOVIDO a `int` antes da operação.
//     Isso significa que `~` de um uint8_t opera sobre 32 bits, não 8.
// ─────────────────────────────────────────────────────────────────────────────
void demo_promocao() {
    fmt::print("\n── 1.2 ⚠ Promoção inteira ──\n");

    const std::uint8_t x = 0x0F;

    // O que a intuição diz: ~0x0F = 0xF0 (8 bits)
    // O que acontece: x vira int (0x0000000F), ~ dá 0xFFFFFFF0 = -16
    const auto resultado = ~x;

    fmt::print("      uint8_t x = 0x0F;\n");
    fmt::print("        tipo de ~x        : {}\n",
               std::is_same_v<std::decay_t<decltype(resultado)>, int> ? "int (!)" : "outro");
    fmt::print("        ~x como int       : {} (0x{:08X})\n", resultado,
               static_cast<std::uint32_t>(resultado));
    fmt::print("        ~x truncado p/ u8 : 0x{:02X}\n",
               static_cast<std::uint8_t>(resultado));
    fmt::print("      ↑ o resultado é NEGATIVO, não 0xF0.\n");

    fmt::print("\n      O bug real que isso causa:\n");
    fmt::print("        uint8_t flags = ...;\n");
    fmt::print("        if (flags == ~MASCARA) ...   // compara u8 com int NEGATIVO\n");
    fmt::print("        → nunca é verdadeiro\n");

    fmt::print("\n      E o caso pior, com deslocamento:\n");
    const std::uint8_t byte = 0xFF;
    const auto deslocado = byte << 4;          // vira int, e não trunca
    fmt::print("        uint8_t(0xFF) << 4 = 0x{:X} ({} bits!), não 0xF0\n",
               deslocado, 32 - __builtin_clz(static_cast<unsigned>(deslocado)));

    fmt::print("\n      As duas defesas:\n");
    fmt::print("        1. use `unsigned` ou `uint32_t` para máscaras\n");
    fmt::print("        2. faça o `static_cast` de volta, explicitamente:\n");
    fmt::print("             x = static_cast<uint8_t>(x & ~M);\n");
    fmt::print("      Compile com -Wconversion para o compilador cobrar isso.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ Shift fora de faixa
// ─────────────────────────────────────────────────────────────────────────────
void demo_shift_ub() {
    fmt::print("\n── 1.3 ⚠ Shift fora de faixa ──\n");

    fmt::print("      A regra: `x << n` e `x >> n` são UB se\n");
    fmt::print("        • n < 0, ou\n");
    fmt::print("        • n >= número de bits de x (APÓS a promoção)\n");

    fmt::print("\n      Os casos que parecem inofensivos e são UB:\n");
    fmt::print("        uint32_t x = 1;  x << 32;    // ✗ UB (32 bits, shift de 32)\n");
    fmt::print("        uint8_t  b = 1;  b << 8;     // ✓ ok! b virou int (32 bits)\n");
    fmt::print("        uint64_t y = 1;  y << 64;    // ✗ UB\n");
    fmt::print("      ↑ a segunda linha é SEGURA por causa da promoção — a mesma\n");
    fmt::print("        promoção que causa problema na seção anterior.\n");

    fmt::print("\n      O idioma perigoso: `1 << n` para n de runtime.\n");
    fmt::print("        uint64_t mask = 1 << 40;      // ✗ `1` é int → UB\n");
    fmt::print("        uint64_t mask = 1ull << 40;   // ✓ o literal é 64 bits\n");

    const std::uint64_t certo = 1ull << 40;
    fmt::print("        1ull << 40 = {} (0x{:X}) ✓\n", certo, certo);

    fmt::print("\n      O UBSan detecta em runtime:\n");
    fmt::print("        runtime error: shift exponent 32 is too large\n");
    fmt::print("      Este demo não executa os casos de UB de propósito.\n");

    // O que MUDOU no C++20: shift à esquerda de negativo deixou de ser UB
    fmt::print("\n      Uma regra que MUDOU no C++20:\n");
    fmt::print("        int x = -1;  x << 1;\n");
    fmt::print("        até C++17 : UB (estourava o bit de sinal)\n");
    fmt::print("        C++20+    : definido — inteiros são complemento de 2\n");
    fmt::print("        (o C++20 fixou complemento de dois como OBRIGATÓRIO)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. `>>` em signed vs unsigned
//
//     Em `unsigned`, o `>>` insere ZEROS à esquerda (shift lógico).
//     Em `signed` negativo, ele insere o BIT DE SINAL (shift aritmético) —
//     comportamento que só ficou garantido no C++20.
// ─────────────────────────────────────────────────────────────────────────────
void demo_shift_direita() {
    fmt::print("\n── 1.4 `>>` em signed vs unsigned ──\n");

    const std::int32_t  negativo   = -16;
    const std::uint32_t sem_sinal  = static_cast<std::uint32_t>(-16);

    fmt::print("      int32_t(-16)  : {}\n", bin(negativo));
    fmt::print("      uint32_t      : {}  (os mesmos bits)\n", bin(sem_sinal));

    fmt::print("\n      após >> 2:\n");
    fmt::print("        signed   : {} = {}\n", bin(negativo >> 2), negativo >> 2);
    fmt::print("        unsigned : {} = {}\n", bin(sem_sinal >> 2), sem_sinal >> 2);

    fmt::print("\n      ↑ o signed preencheu com 1s (o bit de sinal), preservando\n");
    fmt::print("        o valor -4. O unsigned preencheu com 0s e virou um número\n");
    fmt::print("        enorme.\n");

    fmt::print("\n      Consequência: `x >> 1` NÃO é o mesmo que `x / 2` para\n");
    fmt::print("      negativos:\n");
    fmt::print("        -7 >> 1 = {}   (arredonda para BAIXO, para -∞)\n", -7 >> 1);
    fmt::print("        -7 /  2 = {}   (trunca em direção a ZERO)\n", -7 / 2);

    fmt::print("\n      Regra: para manipulação de BITS, use sempre tipos\n");
    fmt::print("      `unsigned`. `signed` é para ARITMÉTICA.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Extrair e inserir campos
//
//     O padrão que aparece em todo protocolo binário e em todo registrador de
//     hardware: um inteiro que empacota vários campos.
// ─────────────────────────────────────────────────────────────────────────────

// Um "registrador" de 32 bits com quatro campos:
//   [31..24] versão   (8 bits)
//   [23..16] tipo     (8 bits)
//   [15.. 4] tamanho (12 bits)
//   [ 3.. 0] flags    (4 bits)
struct CampoBits {
    unsigned deslocamento;
    unsigned largura;

    constexpr std::uint32_t mascara() const {
        return ((1u << largura) - 1u) << deslocamento;
    }
    constexpr std::uint32_t extrair(std::uint32_t reg) const {
        return (reg & mascara()) >> deslocamento;
    }
    constexpr std::uint32_t inserir(std::uint32_t reg, std::uint32_t valor) const {
        return (reg & ~mascara()) | ((valor << deslocamento) & mascara());
    }
};

constexpr CampoBits VERSAO {24, 8};
constexpr CampoBits TIPO   {16, 8};
constexpr CampoBits TAMANHO{ 4, 12};
constexpr CampoBits FLAGS  { 0, 4};

void demo_campos() {
    fmt::print("\n── 1.5 Extrair e inserir campos ──\n");

    std::uint32_t reg = 0;
    reg = VERSAO.inserir(reg, 2);
    reg = TIPO.inserir(reg, 0x1F);
    reg = TAMANHO.inserir(reg, 1024);
    reg = FLAGS.inserir(reg, 0b1010);

    fmt::print("      registrador: {} (0x{:08X})\n", bin(reg), reg);
    fmt::print("        versão  [31..24] = {}\n",   VERSAO.extrair(reg));
    fmt::print("        tipo    [23..16] = 0x{:X}\n", TIPO.extrair(reg));
    fmt::print("        tamanho [15.. 4] = {}\n",   TAMANHO.extrair(reg));
    fmt::print("        flags   [ 3.. 0] = {}\n",   bin(static_cast<std::uint8_t>(FLAGS.extrair(reg))).substr(4));

    // Modificar UM campo sem tocar nos outros
    const auto novo = TAMANHO.inserir(reg, 2048);
    fmt::print("\n      após TAMANHO.inserir(2048):\n");
    fmt::print("        {} \n", bin(novo));
    fmt::print("        tamanho = {} | versão = {} (intacta) | flags = {} (intactas)\n",
               TAMANHO.extrair(novo), VERSAO.extrair(novo), FLAGS.extrair(novo));

    // Tudo constexpr: dá para verificar em compilação
    static_assert(VERSAO.mascara()  == 0xFF000000u);
    static_assert(TAMANHO.mascara() == 0x0000FFF0u);
    static_assert(TAMANHO.extrair(TAMANHO.inserir(0, 4095)) == 4095);
    fmt::print("\n      as máscaras são `constexpr` e verificadas por\n");
    fmt::print("      `static_assert` — erros de offset viram erro de BUILD.\n");

    fmt::print("\n      Compare com campos de bits nativos (demo_bitfields.hpp):\n");
    fmt::print("      esta versão é PORTÁVEL — o layout é definido por você, não\n");
    fmt::print("      pelo compilador.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_mascaras();
    demo_promocao();
    demo_shift_ub();
    demo_shift_direita();
    demo_campos();
}

} // namespace demo_operadores
