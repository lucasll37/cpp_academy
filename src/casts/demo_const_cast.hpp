// =============================================================================
//  src/casts/demo_const_cast.hpp
//
//  const_cast<T> — adiciona ou remove qualificadores const / volatile
//  ──────────────────────────────────────────────────────────────────
//  • O ÚNICO cast que mexe em const/volatile — os outros não conseguem.
//  • Uso legítimo: adaptar código C++ com APIs C legadas que não
//    declaram const onde deveriam.
//  • Uso perigoso: escrever em objeto genuinamente const → UNDEFINED BEHAVIOR.
//  • Regra de ouro: só remova const se você SABE que o objeto não é const.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstring>

namespace demo_const_cast {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Uso legítimo — API C legada que não é const-correct
// ─────────────────────────────────────────────────────────────────────────────

// Simula uma função de biblioteca C que deveria receber const char* mas não recebe
// (erro histórico muito comum em APIs C antigas, como algumas do OpenSSL, etc.)
void api_legada_c(char* buffer, int tamanho) {
    fmt::print("  api_legada_c recebeu: '{}' ({} bytes)\n", buffer, tamanho);
}

void demo_api_legada() {
    fmt::print("\n── 3.1 Adaptando API C legada ──\n");

    const char* mensagem = "hello world";

    // api_legada_c(mensagem, 11); // ✗ erro: não converte const char* → char*

    // const_cast remove o const para satisfazer a assinatura da API
    // SEGURO aqui porque sabemos que api_legada_c não escreve no buffer
    api_legada_c(const_cast<char*>(mensagem), static_cast<int>(std::strlen(mensagem)));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Overload const/não-const — evitar duplicação de código
//     Técnica clássica do Scott Meyers (Effective C++)
// ─────────────────────────────────────────────────────────────────────────────
struct Buffer {
    char dados[64] = "dados do buffer";

    // Versão const (leitura)
    const char& operator[](int i) const {
        fmt::print("  operator[] const chamado\n");
        return dados[i];
    }

    // Versão não-const: chama a versão const para não duplicar lógica
    // Passo 1: cast *this para const Buffer& para chamar a versão const
    // Passo 2: remove o const do resultado (seguro: *this não é const aqui)
    char& operator[](int i) {
        fmt::print("  operator[] não-const chamado (delega para const)\n");
        return const_cast<char&>(
            static_cast<const Buffer&>(*this)[i]
        );
    }
};

void demo_overload_const() {
    fmt::print("\n── 3.2 Overload const/não-const sem duplicação ──\n");

    Buffer b;
    const Buffer cb;

    char& c1 = b[0];              // chama não-const
    const char& c2 = cb[0];      // chama const
    fmt::print("  b[0]  = '{}'\n", c1);
    fmt::print("  cb[0] = '{}'\n", c2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Adicionando const — raro, mas possível
// ─────────────────────────────────────────────────────────────────────────────
void apenas_le(const int* p) {
    fmt::print("  apenas_le: {}\n", *p);
}

void demo_adicionar_const() {
    fmt::print("\n── 3.3 Adicionando const ──\n");

    int valor = 99;
    int* ptr = &valor;

    // Adicionar const: const_cast funciona, mas uma conversão implícita
    // também funcionaria. Aqui é mais explícito.
    apenas_le(const_cast<const int*>(ptr));
    // Equivalente (implícito, mais comum):
    apenas_le(ptr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ UNDEFINED BEHAVIOR — o que NÃO fazer
//     Modificar um objeto que é genuinamente const
// ─────────────────────────────────────────────────────────────────────────────
void demo_ub() {
    fmt::print("\n── 3.4 ⚠  UNDEFINED BEHAVIOR — apenas para entender o perigo ──\n");

    // Este objeto é genuinamente const — o compilador pode colocá-lo
    // em memória somente-leitura (seção .rodata)
    const int CONSTANTE = 42;

    // const_cast remove o const e permite a escrita...
    int* ptr = const_cast<int*>(&CONSTANTE);

    // ...mas ESCREVER aqui é UNDEFINED BEHAVIOR!
    // O compilador pode ignorar a escrita, crashar, ou pior.
    // *ptr = 100;  // ← NUNCA FAÇA ISSO

    fmt::print("  CONSTANTE = {} (o compilador pode usar o valor original mesmo)\n",
               CONSTANTE);
    fmt::print("  Escrever via const_cast em objeto genuinamente const = UB!\n");
    fmt::print("  (linha *ptr = 100 comentada intencionalmente)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. volatile — const_cast também remove volatile
//     (raro em código moderno; mais comum em código embarcado)
// ─────────────────────────────────────────────────────────────────────────────
void demo_volatile() {
    fmt::print("\n── 3.5 Removendo volatile (código embarcado) ──\n");

    // volatile diz ao compilador: "não optimize leituras deste valor,
    // ele pode mudar externamente" (registradores de hardware, etc.)
    volatile int registrador = 0xFF;

    // Para passar para uma função que não aceita volatile:
    int valor = const_cast<int&>(registrador);
    fmt::print("  registrador volatile lido como int: 0x{:X}\n", valor);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_api_legada();
    demo_overload_const();
    demo_adicionar_const();
    demo_ub();
    demo_volatile();
}

} // namespace demo_const_cast
