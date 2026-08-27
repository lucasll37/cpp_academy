// =============================================================================
//  src/layout/demo_tamanho.hpp
//
//  sizeof, alinhamento e padding
//  ─────────────────────────────
//  `sizeof(struct)` quase nunca é a soma dos `sizeof` dos membros. A diferença
//  é o PADDING, e o padding existe por causa do ALINHAMENTO — uma exigência do
//  hardware que o compilador é obrigado a respeitar.
//
//  A regra tem três linhas e explica tudo o que vem depois:
//
//    1. todo tipo tem um alinhamento `alignof(T)`: seu endereço precisa ser
//       múltiplo dele;
//    2. o alinhamento de uma struct é o MAIOR alinhamento entre seus membros;
//    3. `sizeof` é sempre múltiplo de `alignof`, para que arrays funcionem.
//
//  Tópicos:
//  • sizeof ≠ soma dos membros
//  • ⚠ a ordem dos membros muda o tamanho — medido
//  • Por que o padding no FIM também existe
//  • offsetof e o mapa real da struct
//  • Structs vazias e o "1 byte"
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace demo_tamanho {

// ─────────────────────────────────────────────────────────────────────────────
//  1. sizeof ≠ soma dos membros
// ─────────────────────────────────────────────────────────────────────────────
struct Ruim {          // 1 + 8 + 1 + 4 = 14 bytes de dado...
    char      a;
    double    b;
    char      c;
    int       d;
};

struct Bom {           // os MESMOS membros, decrescendo por alinhamento
    double    b;
    int       d;
    char      a;
    char      c;
};

void demo_padding() {
    fmt::print("\n── 1.1 sizeof ≠ soma dos membros ──\n");

    fmt::print("      dados úteis: char + double + char + int = {} bytes\n",
               sizeof(char) * 2 + sizeof(double) + sizeof(int));

    fmt::print("\n      struct Ruim {{ char a; double b; char c; int d; }};\n");
    fmt::print("        sizeof  = {:2}   alignof = {}\n", sizeof(Ruim), alignof(Ruim));
    fmt::print("        offsetof: a={}  b={}  c={:2}  d={:2}\n",
               offsetof(Ruim, a), offsetof(Ruim, b),
               offsetof(Ruim, c), offsetof(Ruim, d));
    fmt::print("        mapa: [a][×7 pad][b b b b b b b b][c][×3 pad][d d d d]\n");

    fmt::print("\n      struct Bom  {{ double b; int d; char a; char c; }};\n");
    fmt::print("        sizeof  = {:2}   alignof = {}\n", sizeof(Bom), alignof(Bom));
    fmt::print("        offsetof: b={}  d={}  a={:2}  c={:2}\n",
               offsetof(Bom, b), offsetof(Bom, d),
               offsetof(Bom, a), offsetof(Bom, c));
    fmt::print("        mapa: [b b b b b b b b][d d d d][a][c][×2 pad]\n");

    const auto economia = sizeof(Ruim) - sizeof(Bom);
    fmt::print("\n      MESMOS dados, {} bytes de diferença ({:.0f}% menor).\n",
               economia, 100.0 * static_cast<double>(economia) / sizeof(Ruim));
    fmt::print("      Num vetor de 1 milhão: {:.1f} MB contra {:.1f} MB.\n",
               sizeof(Ruim) * 1e6 / 1048576.0, sizeof(Bom) * 1e6 / 1048576.0);

    fmt::print("\n      Regra prática: declare os membros em ordem DECRESCENTE de\n");
    fmt::print("      alinhamento. Não é otimização prematura — é gratuito, e o\n");
    fmt::print("      compilador NÃO pode fazer por você (a ordem de declaração é\n");
    fmt::print("      garantida pelo padrão para membros com o mesmo access\n");
    fmt::print("      specifier, justamente para permitir layout previsível).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Por que existe padding no FIM
// ─────────────────────────────────────────────────────────────────────────────
struct ComRabo { int  a; char b; };   // 4 + 1 = 5, mas sizeof é 8

void demo_padding_final() {
    fmt::print("\n── 1.2 O padding do fim ──\n");

    fmt::print("      struct ComRabo {{ int a; char b; }};\n");
    fmt::print("        soma dos membros = 5, sizeof = {}, alignof = {}\n",
               sizeof(ComRabo), alignof(ComRabo));

    fmt::print("\n      Por quê? Por causa dos ARRAYS. Num `ComRabo v[3]`, cada\n");
    fmt::print("      elemento precisa começar num endereço múltiplo de {}.\n",
               alignof(ComRabo));
    fmt::print("      Se sizeof fosse 5, v[1] cairia no offset 5 — desalinhado.\n");
    fmt::print("      Daí a regra: sizeof é SEMPRE múltiplo de alignof.\n\n");

    ComRabo v[3]{};
    const auto base = reinterpret_cast<std::uintptr_t>(&v[0]);
    for (int i = 0; i < 3; ++i)
        fmt::print("        &v[{}] - &v[0] = {}   (múltiplo de {}? {})\n", i,
                   reinterpret_cast<std::uintptr_t>(&v[i]) - base,
                   alignof(ComRabo),
                   (reinterpret_cast<std::uintptr_t>(&v[i]) - base) % alignof(ComRabo) == 0);

    fmt::print("\n      ⚠  Consequência importante: os bytes de padding têm valor\n");
    fmt::print("         INDETERMINADO. Comparar duas structs com `memcmp` pode\n");
    fmt::print("         dar diferente mesmo com todos os membros iguais.\n");
    fmt::print("         Use `operator==` (ou `= default`, ver src/operators/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O padding é observável — medindo com memcmp
// ─────────────────────────────────────────────────────────────────────────────
void demo_memcmp() {
    fmt::print("\n── 1.3 ⚠ memcmp e o lixo do padding ──\n");

    // Duas instâncias com os MESMOS valores de membro. A diferença está só
    // nos bytes de padding — e nós os marcamos de propósito, para que o
    // resultado seja o mesmo em toda execução. (Deixar `x` sem inicializar
    // também "funcionaria", mas o padding poderia calhar de estar zerado e o
    // demo passaria a mentir em algumas execuções.)
    ComRabo x;
    std::memset(&x, 0xAA, sizeof x);   // legítimo: ComRabo é trivialmente copiável
    x.a = 7; x.b = 'z';

    ComRabo y{};                       // value-init: TUDO zerado, padding incluso
    y.a = 7; y.b = 'z';

    const bool iguais_por_membro = (x.a == y.a && x.b == y.b);
    const bool iguais_por_bytes  = std::memcmp(&x, &y, sizeof(ComRabo)) == 0;

    fmt::print("      x.a == y.a && x.b == y.b     → {}\n", iguais_por_membro);
    fmt::print("      memcmp(&x, &y, sizeof)  == 0 → {}\n", iguais_por_bytes);
    fmt::print("      os {} bytes, um a um:\n", sizeof(ComRabo));
    {
        const auto* bx = reinterpret_cast<const unsigned char*>(&x);
        const auto* by = reinterpret_cast<const unsigned char*>(&y);
        fmt::print("        x: ");
        for (std::size_t k = 0; k < sizeof(ComRabo); ++k) fmt::print("{:02X} ", bx[k]);
        fmt::print("\n        y: ");
        for (std::size_t k = 0; k < sizeof(ComRabo); ++k) fmt::print("{:02X} ", by[k]);
        fmt::print("\n           {:{}}▲▲ ▲▲ ▲▲  ← padding\n", "", 3 * 5);
    }
    fmt::print("      Os membros são iguais e os BYTES não são. O padding NÃO faz\n");
    fmt::print("      parte do valor do objeto — e o seu conteúdo é indeterminado.\n");

    fmt::print("\n      Por isso:\n");
    fmt::print("        • NUNCA use memcmp para comparar structs;\n");
    fmt::print("        • NUNCA transmita/persista uma struct com padding sem\n");
    fmt::print("          zerá-la antes — o padding vaza memória do processo\n");
    fmt::print("          (foi a classe de bug do Heartbleed, em outra forma);\n");
    fmt::print("        • se precisa de bytes, serialize campo a campo\n");
    fmt::print("          (ver src/bits/demo_endianness.hpp).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Structs vazias
// ─────────────────────────────────────────────────────────────────────────────
struct Vazia { };
struct SoMetodos { void f() const {} };
struct HerdaVazia : Vazia { int x; };

void demo_vazia() {
    fmt::print("\n── 1.4 Structs vazias ──\n");

    fmt::print("      sizeof(struct {{ }})               = {}\n", sizeof(Vazia));
    fmt::print("      sizeof(struct {{ void f(); }})     = {}\n", sizeof(SoMetodos));

    fmt::print("\n      Zero seria mais honesto, mas quebraria a linguagem: dois\n");
    fmt::print("      objetos distintos precisam de ENDEREÇOS distintos, e num\n");
    fmt::print("      array `Vazia v[3]` os três seriam o mesmo endereço.\n");
    {
        Vazia v[3];
        fmt::print("        &v[0]={}  &v[1]-&v[0]={}  distintos? {}\n",
                   static_cast<const void*>(&v[0]),
                   reinterpret_cast<std::uintptr_t>(&v[1])
                     - reinterpret_cast<std::uintptr_t>(&v[0]),
                   &v[0] != &v[1]);
    }

    fmt::print("\n      Métodos não ocupam espaço no objeto: eles são funções\n");
    fmt::print("      comuns com um `this` implícito, guardadas UMA vez no\n");
    fmt::print("      binário. Só `virtual` acrescenta algo (o vptr, seção 5).\n\n");

    fmt::print("      A exceção que salva o dia — herança:\n");
    fmt::print("        struct HerdaVazia : Vazia {{ int x; }};\n");
    fmt::print("        sizeof = {} (não 8!)\n", sizeof(HerdaVazia));
    fmt::print("      A base vazia foi otimizada para 0 bytes. Isso é a EBO —\n");
    fmt::print("      Empty Base Optimization — e é o assunto da seção 4.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O mapa de um tipo real da biblioteca padrão
// ─────────────────────────────────────────────────────────────────────────────
void demo_padrao() {
    fmt::print("\n── 1.5 Tipos da biblioteca padrão ──\n");

    struct Linha { const char* nome; std::size_t tam; std::size_t alin; };
    const Linha linhas[] = {
        {"char",                 sizeof(char),                 alignof(char)},
        {"int",                  sizeof(int),                  alignof(int)},
        {"double",               sizeof(double),               alignof(double)},
        {"long double",          sizeof(long double),          alignof(long double)},
        {"void*",                sizeof(void*),                alignof(void*)},
        {"std::string",          sizeof(std::string),          alignof(std::string)},
        {"std::vector<int>",     sizeof(std::vector<int>),     alignof(std::vector<int>)},
        {"std::max_align_t",     sizeof(std::max_align_t),     alignof(std::max_align_t)},
    };

    fmt::print("      | tipo               | sizeof | alignof |\n");
    fmt::print("      |--------------------|--------|---------|\n");
    for (const auto& l : linhas)
        fmt::print("      | {:<18} | {:6} | {:7} |\n", l.nome, l.tam, l.alin);

    fmt::print("\n      `long double` no x86-64 SysV é o caso curioso: 80 bits de\n");
    fmt::print("      dado útil, alinhamento de 16, sizeof {} — quase metade é\n",
               sizeof(long double));
    fmt::print("      padding. No MSVC ele é 8 bytes (igual a double).\n\n");

    fmt::print("      `std::max_align_t` é o alinhamento que `malloc` garante:\n");
    fmt::print("      {} bytes. Alocar algo com alinhamento maior exige\n",
               alignof(std::max_align_t));
    fmt::print("      `aligned_alloc`, `new(std::align_val_t{{N}})` ou `alignas` —\n");
    fmt::print("      assunto da seção 2.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_padding();
    demo_padding_final();
    demo_memcmp();
    demo_vazia();
    demo_padrao();
}

}  // namespace demo_tamanho
