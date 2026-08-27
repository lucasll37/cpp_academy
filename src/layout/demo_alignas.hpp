// =============================================================================
//  src/layout/demo_alignas.hpp
//
//  alignas, alignof e alinhamento sob demanda
//  ─────────────────────────────────────────
//  O compilador escolhe o alinhamento natural de cada tipo. Há três situações
//  em que você precisa mandar nisso:
//
//    • SIMD          — `__m256` exige 32 bytes; carregar desalinhado é falha
//    • cache         — separar dois campos em linhas distintas (false sharing)
//    • hardware/ABI  — um descritor de DMA que o dispositivo exige alinhado
//
//  Tópicos:
//  • alignas em tipo e em membro
//  • ⚠ alignas só AUMENTA — nunca diminui
//  • Alocação super-alinhada: o operator new do C++17
//  • alignof(std::max_align_t) e o limite do malloc
//  • assume_aligned e o que o compilador faz com a informação
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace demo_alignas {

// ─────────────────────────────────────────────────────────────────────────────
//  1. alignas em tipo e em membro
// ─────────────────────────────────────────────────────────────────────────────
struct alignas(32) Vetor8f { float v[8]; };     // pronto para AVX

struct Espacada {
    alignas(64) int produtor;                   // cada um na sua linha de cache
    alignas(64) int consumidor;
};

struct Compacta { int produtor; int consumidor; };

void demo_basico() {
    fmt::print("\n── 2.1 alignas ──\n");

    fmt::print("      struct alignas(32) Vetor8f {{ float v[8]; }};\n");
    fmt::print("        sizeof = {}, alignof = {}\n", sizeof(Vetor8f), alignof(Vetor8f));
    fmt::print("        (sem o alignas seria alignof = {}, o do float)\n", alignof(float));

    fmt::print("\n      alignas em MEMBRO — separando linhas de cache:\n");
    fmt::print("        struct Compacta  → sizeof {:3}, alignof {:2}\n",
               sizeof(Compacta), alignof(Compacta));
    fmt::print("        struct Espacada  → sizeof {:3}, alignof {:2}\n",
               sizeof(Espacada), alignof(Espacada));
    fmt::print("        offsetof(Espacada, consumidor) = {}\n",
               offsetof(Espacada, consumidor));

    fmt::print("\n      Custou {}× mais memória para eliminar o false sharing\n",
               sizeof(Espacada) / sizeof(Compacta));
    fmt::print("      entre dois campos escritos por threads diferentes.\n");
    fmt::print("      Se compensa, é medição — ver src/performance/.\n");

    // Verificando na prática
    Vetor8f arr[3]{};
    fmt::print("\n      endereços reais de Vetor8f arr[3]:\n");
    for (int i = 0; i < 3; ++i) {
        const auto e = reinterpret_cast<std::uintptr_t>(&arr[i]);
        fmt::print("        &arr[{}] % 32 = {}\n", i, e % 32);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ alignas só aumenta
// ─────────────────────────────────────────────────────────────────────────────
void demo_so_aumenta() {
    fmt::print("\n── 2.2 ⚠ alignas só AUMENTA ──\n");

    fmt::print("      struct alignas(1) Tentativa {{ double d; }};\n");
    fmt::print("      → error: alignment of 'Tentativa' is less than the minimum\n");
    fmt::print("        alignment (8) of its members\n\n");

    fmt::print("      Não há como pedir alinhamento MENOR que o natural com\n");
    fmt::print("      `alignas`. Isso é proposital: ler um `double` desalinhado é\n");
    fmt::print("      UB, e em ARM antigo é SIGBUS de verdade.\n\n");

    fmt::print("      Para empacotar mesmo, existem extensões NÃO PADRÃO:\n");
    fmt::print("        #pragma pack(1)               (MSVC e GCC)\n");
    fmt::print("        __attribute__((packed))       (GCC/Clang)\n\n");

    fmt::print("      ⚠  Elas criam um perigo silencioso: um ponteiro para um\n");
    fmt::print("         membro de struct empacotada é DESALINHADO, e passá-lo\n");
    fmt::print("         adiante é UB. O GCC avisa com -Waddress-of-packed-member.\n");
    fmt::print("         Se você precisa de bytes exatos (protocolo de rede,\n");
    fmt::print("         formato de arquivo), a saída correta é serializar campo a\n");
    fmt::print("         campo — src/bits/demo_endianness.hpp.\n\n");

    fmt::print("      `alignas(0)` é permitido e significa \"ignore\", não \"zero\".\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Alocação super-alinhada
// ─────────────────────────────────────────────────────────────────────────────
void demo_alocacao() {
    fmt::print("\n── 2.3 Alocação super-alinhada ──\n");

    fmt::print("      `malloc` garante alinhamento de alignof(max_align_t) = {}.\n",
               alignof(std::max_align_t));
    fmt::print("      Um tipo que peça mais que isso é \"over-aligned\".\n\n");

    fmt::print("      Antes do C++17, `new Vetor8f` IGNORAVA o alignas(32) e\n");
    fmt::print("      devolvia memória alinhada a {} — UB silencioso ao usar AVX.\n",
               alignof(std::max_align_t));
    fmt::print("      O C++17 (P0035) acrescentou as sobrecargas com\n");
    fmt::print("      `std::align_val_t`, e o compilador as chama sozinho:\n\n");

    // Medindo se o new realmente respeita
    constexpr int kAmostras = 8;
    int alinhados = 0;
    std::vector<std::unique_ptr<Vetor8f>> ps;
    for (int i = 0; i < kAmostras; ++i) {
        ps.push_back(std::make_unique<Vetor8f>());
        if (reinterpret_cast<std::uintptr_t>(ps.back().get()) % 32 == 0) ++alinhados;
    }
    fmt::print("        {} de {} alocações de Vetor8f (alignas 32) vieram\n",
               alinhados, kAmostras);
    fmt::print("        alinhadas a 32 bytes.\n");
    fmt::print("        (o compilador emitiu `operator new(size, align_val_t{{32}})`)\n");

    fmt::print("\n      Alinhando à mão, três formas:\n");
    fmt::print("        std::aligned_alloc(32, n)        // C11, n múltiplo de 32\n");
    fmt::print("        ::operator new(n, std::align_val_t{{32}})\n");
    fmt::print("        std::align(32, n, ptr, espaco)   // ajusta um bloco existente\n\n");

    // std::align na prática
    alignas(1) unsigned char bruto[128];
    void*       p      = bruto + 1;              // de propósito desalinhado
    std::size_t espaco = sizeof(bruto) - 1;
    void*       ajustado = std::align(32, 32, p, espaco);
    fmt::print("      std::align: resto do endereço mod 32 — antes {}, depois {}\n",
               reinterpret_cast<std::uintptr_t>(bruto + 1) % 32,
               ajustado ? reinterpret_cast<std::uintptr_t>(ajustado) % 32 : 999);
    fmt::print("      (sobraram {} bytes utilizáveis dos {})\n", espaco, sizeof(bruto) - 1);

    fmt::print("\n      ⚠  memória obtida com `aligned_alloc` se libera com `free`;\n");
    fmt::print("         com `operator new(align_val_t)`, com o `operator delete`\n");
    fmt::print("         correspondente. Misturar é UB.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. assume_aligned
// ─────────────────────────────────────────────────────────────────────────────
void demo_assume() {
    fmt::print("\n── 2.4 std::assume_aligned (C++20) ──\n");

    fmt::print("      O tipo carrega o alinhamento; um `float*` cru não carrega\n");
    fmt::print("      nada. `std::assume_aligned<N>(p)` PROMETE ao compilador que\n");
    fmt::print("      `p` está alinhado a N, liberando vetorização melhor:\n\n");
    fmt::print("        void escala(float* p, std::size_t n) {{\n");
    fmt::print("            auto* q = std::assume_aligned<32>(p);\n");
    fmt::print("            for (std::size_t i = 0; i < n; ++i) q[i] *= 2.0f;\n");
    fmt::print("        }}\n\n");

    fmt::print("      Sem a promessa, o compilador gera um prólogo escalar até\n");
    fmt::print("      alcançar o alinhamento, depois o laço vetorizado, depois um\n");
    fmt::print("      epílogo. Com ela, só o laço vetorizado.\n\n");

    fmt::print("      ⚠  É uma PROMESSA, não uma verificação. Mentir é UB, e do\n");
    fmt::print("         pior tipo: funciona no x86 (que tolera desalinhamento em\n");
    fmt::print("         muitas instruções) e falha no ARM.\n");
    fmt::print("         O UBSan pega isso com -fsanitize=alignment.\n\n");

    alignas(32) float dados[64]{};
    auto* q = std::assume_aligned<32>(dados);
    fmt::print("      verificação: dados % 32 = {}, promessa honesta? {}\n",
               reinterpret_cast<std::uintptr_t>(dados) % 32,
               reinterpret_cast<std::uintptr_t>(q) % 32 == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_so_aumenta();
    demo_alocacao();
    demo_assume();
}

}  // namespace demo_alignas
