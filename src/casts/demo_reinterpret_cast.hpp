// =============================================================================
//  src/casts/demo_reinterpret_cast.hpp
//
//  reinterpret_cast<T> — reinterpretação bruta de bits/bytes
//  ──────────────────────────────────────────────────────────
//  • Não converte — apenas reinterpreta os mesmos bytes como outro tipo.
//  • O cast mais poderoso e o mais perigoso.
//  • Não há verificação alguma (compile-time nem runtime).
//  • Usos legítimos: serialização, interoperabilidade com hardware,
//    ponteiro ↔ inteiro, type punning via memcpy (não via cast direto!).
//  • Viola aliasing rules se usado errado → Undefined Behavior.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdint>
#include <cstring>
#include <bit>       // std::bit_cast (C++20 — alternativa mais segura)
#include <climits>

namespace demo_reinterpret_cast {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Ponteiro → inteiro e de volta
//     Uso legítimo: armazenar endereço em inteiro, calcular offsets
// ─────────────────────────────────────────────────────────────────────────────
void demo_ptr_para_inteiro() {
    fmt::print("\n── 4.1 Ponteiro ↔ inteiro ──\n");

    int valor = 42;
    int* ptr = &valor;

    // Ponteiro → inteiro sem sinal (tamanho garantido suficiente)
    uintptr_t endereco = reinterpret_cast<uintptr_t>(ptr);
    fmt::print("  endereço como uintptr_t = 0x{:X}\n", endereco);
    fmt::print("  alinhamento (endereco % 4 == 0)? {}\n", endereco % 4 == 0);

    // De volta para ponteiro
    int* recuperado = reinterpret_cast<int*>(endereco);
    fmt::print("  valor recuperado = {}\n", *recuperado);

    // ⚠  int* não é garantidamente do mesmo tamanho que int em todas as plataformas
    //    Por isso usamos uintptr_t (definido em <cstdint>)
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Inspecionar bytes de um tipo — "ver por baixo dos panos"
// ─────────────────────────────────────────────────────────────────────────────
void demo_inspecionar_bytes() {
    fmt::print("\n── 4.2 Inspecionar bytes de um tipo ──\n");

    // Representação de float em IEEE 754
    float f = 1.0f;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&f);

    fmt::print("  float 1.0f em bytes (little-endian): ");
    for (std::size_t i = 0; i < sizeof(float); ++i) {
        fmt::print("{:02X} ", bytes[i]);
    }
    fmt::print("\n");
    // 00 00 80 3F em little-endian = 0x3F800000 = IEEE 754 para 1.0

    // Mesmo para int
    int n = 0xDEADBEEF;
    const uint8_t* nb = reinterpret_cast<const uint8_t*>(&n);
    fmt::print("  int 0xDEADBEEF em bytes: ");
    for (std::size_t i = 0; i < sizeof(int); ++i) {
        fmt::print("{:02X} ", nb[i]);
    }
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Type punning — ler bits de float como uint32_t
//     FORMA CORRETA: via memcpy (evita strict aliasing UB)
//     FORMA ERRADA:  via reinterpret_cast direto (UB!)
// ─────────────────────────────────────────────────────────────────────────────
void demo_type_punning() {
    fmt::print("\n── 4.3 Type punning (float ↔ uint32_t) ──\n");

    float pi = 3.14159f;

    // ✗ ERRADO — strict aliasing violation (UB, mesmo que "funcione")
    // uint32_t bits_errado = *reinterpret_cast<uint32_t*>(&pi);

    // ✓ CORRETO — memcpy é garantidamente seguro para type punning
    uint32_t bits_correto;
    std::memcpy(&bits_correto, &pi, sizeof(float));
    fmt::print("  pi = {:.5f}\n", pi);
    fmt::print("  bits via memcpy = 0x{:08X}\n", bits_correto);

    // ✓ MELHOR ainda em C++20: std::bit_cast (compile-time safe)
    // uint32_t bits_bitcast = std::bit_cast<uint32_t>(pi);

    // Aplicação prática: fast inverse square root (famoso hack do Quake III)
    // https://en.wikipedia.org/wiki/Fast_inverse_square_root
    float x = 4.0f;
    uint32_t i;
    std::memcpy(&i, &x, sizeof(float));
    fmt::print("  float {:.1f} como bits = 0x{:08X} (base do hack Q3)\n", x, i);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Serialização/Deserialização de structs
//     Comum em protocolos de rede, arquivos binários
// ─────────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)   // sem padding — bytes exatos
struct PacoteRede {
    uint8_t  versao;
    uint16_t tamanho;
    uint32_t checksum;
};
#pragma pack(pop)

void demo_serializacao() {
    fmt::print("\n── 4.4 Serialização de struct para bytes ──\n");

    PacoteRede pkt { .versao = 2, .tamanho = 128, .checksum = 0xCAFEBABE };
    fmt::print("  struct PacoteRede: versao={}, tamanho={}, checksum=0x{:X}\n",
               pkt.versao, pkt.tamanho, pkt.checksum);

    // Serializar para buffer de bytes (envio via socket, etc.)
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&pkt);
    fmt::print("  bytes serializados ({}B): ", sizeof(PacoteRede));
    for (std::size_t i = 0; i < sizeof(PacoteRede); ++i) {
        fmt::print("{:02X} ", raw[i]);
    }
    fmt::print("\n");

    // Deserializar de volta — simula recepção de bytes de rede
    uint8_t buffer[] = { 0x02, 0x80, 0x00, 0xBE, 0xBA, 0xFE, 0xCA };
    //                   ver  size(LE) checksum(LE)
    const PacoteRede* recebido = reinterpret_cast<const PacoteRede*>(buffer);
    fmt::print("  deserializado: versao={}, tamanho={}, checksum=0x{:X}\n",
               recebido->versao, recebido->tamanho, recebido->checksum);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Interop com APIs C que usam void* para callbacks genéricos
// ─────────────────────────────────────────────────────────────────────────────
struct Contexto {
    int id;
    const char* nome;
};

// Simula callback estilo C (ex: pthread, timers, GUI frameworks)
void callback_c(void* user_data) {
    // Recupera o contexto original
    Contexto* ctx = reinterpret_cast<Contexto*>(user_data);
    fmt::print("  callback: id={}, nome='{}'\n", ctx->id, ctx->nome);
}

void demo_callback_void_ptr() {
    fmt::print("\n── 4.5 Callback com void* (padrão C de extensibilidade) ──\n");

    Contexto ctx { 7, "sensor_temperatura" };

    // Passa contexto tipado como void*
    void* user_data = reinterpret_cast<void*>(&ctx);
    callback_c(user_data);
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. O que reinterpret_cast NÃO pode fazer
// ─────────────────────────────────────────────────────────────────────────────
void demo_limitacoes() {
    fmt::print("\n── 4.6 Limitações do reinterpret_cast ──\n");
    fmt::print(
        "  ✗ Não converte entre tipos aritméticos: int→double (use static_cast)\n"
        "  ✗ Não faz downcast em hierarquias     (use dynamic/static_cast)\n"
        "  ✗ Não remove const                    (use const_cast)\n"
        "  ✗ Não garante alinhamento correto — acessar T* mal-alinhado = UB\n"
        "  ✗ Viola strict aliasing se ler tipo diferente do original\n"
        "  ✓ Ponteiro/referência ↔ inteiro\n"
        "  ✓ Inspecionar bytes (via char* / uint8_t* — exceção de aliasing)\n"
        "  ✓ Interop com APIs C (void* callbacks)\n"
        "  ✓ Serialização de structs com layout conhecido\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_ptr_para_inteiro();
    demo_inspecionar_bytes();
    demo_type_punning();
    demo_serializacao();
    demo_callback_void_ptr();
    demo_limitacoes();
}

} // namespace demo_reinterpret_cast
