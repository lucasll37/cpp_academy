// =============================================================================
//  src/undefined_behavior/ub_exemplos.cpp
//
//  ⚠⚠  ESTE ARQUIVO CONTÉM COMPORTAMENTO INDEFINIDO DE PROPÓSITO.  ⚠⚠
//
//  Ele existe para ser executado SOB SANITIZER, e cada caso é disparado por
//  um argumento explícito — nada roda por acidente. É a única forma honesta
//  de mostrar o que os sanitizers pegam: descrever a saída de memória seria
//  chutar, e este repositório mede.
//
//      ninja -C build src/undefined_behavior/ub_exemplos
//      ./build/src/undefined_behavior/ub_exemplos            # lista os casos
//      ./build/src/undefined_behavior/ub_exemplos heap       # roda um
//
//  Para ver o diagnóstico, compile com sanitizer:
//      g++ -std=c++20 -O1 -g -fsanitize=address,undefined
//          src/undefined_behavior/ub_exemplos.cpp -o /tmp/ub
//      /tmp/ub heap
//
//  Este alvo NÃO é instalado em dist/bin — ver o meson.build.
// =============================================================================
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// As funções abaixo são `noinline` e recebem os valores por parâmetro para que
// o compilador não consiga provar o UB em compilação e simplesmente apagar o
// código — o que faria o demo não demonstrar nada.
#define RUIDO __attribute__((noinline))

// ── 1. heap-buffer-overflow ─────────────────────────────────────────────────
RUIDO int caso_heap(int indice) {
    std::vector<int> v(4, 7);
    return v[static_cast<std::size_t>(indice)];   // indice == 10 → fora
}

// ── 2. stack-buffer-overflow ────────────────────────────────────────────────
RUIDO int caso_pilha(int indice) {
    int a[4] = {1, 2, 3, 4};
    return a[indice];
}

// ── 3. use-after-free ───────────────────────────────────────────────────────
RUIDO int caso_uaf(int n) {
    int* p = new int[4];
    p[0] = n;
    delete[] p;
    return p[0];                                  // ← lê memória liberada
}

// ── 4. use-after-scope: a referência para um temporário ──────────────────────
// O -Wreturn-local-addr AQUI é a lição: o compilador já sabe que isto é UB e
// avisa antes de qualquer sanitizer. Suprimido porque o arquivo existe para
// EXECUTAR o erro — em código de verdade, o aviso deve ser corrigido.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
RUIDO const std::string& caso_dangling_impl() {
    std::string local = "some ao sair";
    return local;                                 // ← devolve ref para local
}
#pragma GCC diagnostic pop
RUIDO std::size_t caso_dangling() {
    const std::string& r = caso_dangling_impl();
    return r.size();
}

// ── 5. overflow de inteiro COM SINAL (UB; sem sinal seria definido) ─────────
RUIDO int caso_overflow(int a, int b) {
    return a + b;                                 // INT_MAX + 1
}

// ── 6. shift fora de faixa ──────────────────────────────────────────────────
RUIDO int caso_shift(int v, int n) {
    return v << n;                                // n == 40 num int de 32 bits
}

// ── 7. divisão por zero inteira ─────────────────────────────────────────────
RUIDO int caso_divisao(int a, int b) {
    return a / b;
}

// ── 8. strict aliasing: ler um float como se fosse int ──────────────────────
RUIDO int caso_aliasing(float f) {
    float x = f;
    return *reinterpret_cast<int*>(&x);           // ← viola strict aliasing
}

// ── 9. leitura de variável não inicializada ─────────────────────────────────
RUIDO int caso_nao_inicializada(bool ligar) {
    int x;                                        // sem inicializar
    if (ligar) x = 1;
    return x;                                     // se !ligar → indeterminado
}

// ── 10. desreferência de ponteiro nulo ──────────────────────────────────────
RUIDO int caso_nulo(int* p) {
    return *p;
}

// ── 11. vazamento (não é UB, mas o LeakSanitizer pega) ──────────────────────
RUIDO void caso_vazamento() {
    auto* p = new int[1000];
    p[0] = 1;
    (void)p;                                      // nunca liberado
}

// ── 12. desalinhamento ──────────────────────────────────────────────────────
RUIDO int caso_alinhamento() {
    alignas(8) unsigned char buf[16] = {};
    // buf+1 não é múltiplo de 4: ler um int daí é UB de alinhamento
    return *reinterpret_cast<int*>(buf + 1);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::puts("⚠ Este programa executa COMPORTAMENTO INDEFINIDO de propósito.");
        std::puts("  Compile com -fsanitize=address,undefined e rode um caso:\n");
        std::puts("    heap          heap-buffer-overflow");
        std::puts("    pilha         stack-buffer-overflow");
        std::puts("    uaf           use-after-free");
        std::puts("    dangling      referência para objeto morto");
        std::puts("    overflow      overflow de int com sinal");
        std::puts("    shift         shift >= largura do tipo");
        std::puts("    divisao       divisão inteira por zero");
        std::puts("    aliasing      violação de strict aliasing");
        std::puts("    naoinit       leitura de não inicializado");
        std::puts("    nulo          desreferência de nullptr");
        std::puts("    vazamento     memória nunca liberada");
        std::puts("    alinhamento   leitura desalinhada");
        return 0;
    }

    const std::string caso = argv[1];
    // `std::atoi` do argv impede o compilador de constant-folding o UB.
    const int arg = argc > 2 ? std::atoi(argv[2]) : 0;

    if (caso == "heap")        std::printf("%d\n", caso_heap(10 + arg));
    else if (caso == "pilha")  std::printf("%d\n", caso_pilha(9 + arg));
    else if (caso == "uaf")    std::printf("%d\n", caso_uaf(42 + arg));
    else if (caso == "dangling") std::printf("%zu\n", caso_dangling());
    else if (caso == "overflow") std::printf("%d\n", caso_overflow(2147483647, 1 + arg));
    else if (caso == "shift")  std::printf("%d\n", caso_shift(1, 40 + arg));
    else if (caso == "divisao") std::printf("%d\n", caso_divisao(10, arg));
    else if (caso == "aliasing") std::printf("%d\n", caso_aliasing(3.14f));
    else if (caso == "naoinit") std::printf("%d\n", caso_nao_inicializada(arg != 0));
    else if (caso == "nulo")   std::printf("%d\n", caso_nulo(nullptr));
    else if (caso == "vazamento") { caso_vazamento(); std::puts("alocou e não liberou"); }
    else if (caso == "alinhamento") std::printf("%d\n", caso_alinhamento());
    else { std::printf("caso desconhecido: %s\n", caso.c_str()); return 1; }

    return 0;
}
