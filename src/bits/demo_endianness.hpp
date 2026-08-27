// =============================================================================
//  src/bits/demo_endianness.hpp
//
//  Endianness e serialização binária
//  ─────────────────────────────────
//  Um `uint32_t` com o valor 0x12345678 ocupa 4 bytes na memória. Em que ORDEM
//  esses bytes aparecem depende da arquitetura:
//
//    little-endian (x86, ARM padrão) : 78 56 34 12   ← menos significativo 1º
//    big-endian    (rede, PowerPC)   : 12 34 56 78   ← mais significativo 1º
//
//  Enquanto os dados ficam na memória do processo, isso é invisível. Assim que
//  eles viram BYTES — arquivo, socket, memória compartilhada — a ordem passa a
//  fazer parte do formato, e escolher errado corrompe tudo silenciosamente.
//
//  Tópicos:
//  • std::endian — descobrir a ordem nativa em compilação
//  • Ver os bytes de verdade
//  • std::byteswap (C++23) e as alternativas
//  • Serialização portável — a única forma correta
//  • ⚠ Por que `memcpy` do struct inteiro está errado
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_endianness {

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::endian — a ordem nativa, em compilação
// ─────────────────────────────────────────────────────────────────────────────
void demo_endian() {
    fmt::print("\n── 5.1 std::endian ──\n");

    if constexpr (std::endian::native == std::endian::little) {
        fmt::print("      esta máquina é LITTLE-endian\n");
    } else if constexpr (std::endian::native == std::endian::big) {
        fmt::print("      esta máquina é BIG-endian\n");
    } else {
        fmt::print("      esta máquina é MISTA (raríssimo)\n");
    }

    fmt::print("      std::endian::native == little ? {}\n",
               std::endian::native == std::endian::little);

    fmt::print("\n      Antes do C++20, descobrir isso exigia um truque de\n");
    fmt::print("      runtime, e ele era tecnicamente UB:\n");
    fmt::print("        uint16_t x = 1;\n");
    fmt::print("        bool little = *(char*)&x == 1;   // ✗ strict aliasing\n");
    fmt::print("      Com `std::endian`, é uma constante de COMPILAÇÃO — dá para\n");
    fmt::print("      usar em `if constexpr` e podar o ramo errado.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Ver os bytes de verdade
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
std::string bytes_de(T v) {
    // `bit_cast` para um array de bytes: correto e constexpr, ao contrário de
    // `reinterpret_cast` (strict aliasing) e da union (membro inativo).
    const auto b = std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(v);
    std::string r;
    for (auto x : b) r += fmt::format("{:02X} ", x);
    return r;
}

void demo_ver_bytes() {
    fmt::print("\n── 5.2 Os bytes na memória ──\n");

    const std::uint32_t v32 = 0x12345678u;
    const std::uint16_t v16 = 0xABCDu;
    const std::uint64_t v64 = 0x0102030405060708ull;
    const float         f   = 1.0f;

    fmt::print("      uint32_t 0x12345678 → {}\n", bytes_de(v32));
    fmt::print("      uint16_t 0xABCD     → {}\n", bytes_de(v16));
    fmt::print("      uint64_t 0x0102…08  → {}\n", bytes_de(v64));
    fmt::print("      float    1.0f       → {}\n", bytes_de(f));

    fmt::print("\n      ↑ os bytes aparecem na ordem INVERSA do valor escrito.\n");
    fmt::print("        É isso que little-endian significa: o byte menos\n");
    fmt::print("        significativo fica no MENOR endereço.\n");

    fmt::print("\n      E note que o endianness afeta a ordem dos BYTES, não a\n");
    fmt::print("      dos BITS dentro de cada byte — essa é sempre a mesma.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Trocar a ordem
// ─────────────────────────────────────────────────────────────────────────────
void demo_byteswap() {
    fmt::print("\n── 5.3 Trocar a ordem dos bytes ──\n");

    const std::uint32_t v = 0x12345678u;

#if defined(__cpp_lib_byteswap)
    const auto trocado = std::byteswap(v);
    fmt::print("      std::byteswap (C++23) disponível ✓\n");
#else
    // Fallback: o builtin do GCC/Clang
    const auto trocado = __builtin_bswap32(v);
    fmt::print("      std::byteswap não disponível — usando __builtin_bswap32\n");
    fmt::print("      (__cpp_lib_byteswap não está definido neste toolchain)\n");
#endif

    fmt::print("        original: 0x{:08X} → {}\n", v, bytes_de(v));
    fmt::print("        trocado : 0x{:08X} → {}\n", trocado, bytes_de(trocado));

    fmt::print("\n      As formas, da melhor para a pior:\n");
    fmt::print("        std::byteswap(v)      C++23, portável, uma instrução\n");
    fmt::print("        __builtin_bswapNN(v)  GCC/Clang, uma instrução (BSWAP)\n");
    fmt::print("        htonl/ntohl           POSIX, só 16/32 bits, só p/ rede\n");
    fmt::print("        shifts à mão          portável e feio; o compilador\n");
    fmt::print("                              geralmente reconhece o idioma\n");

    // A versão à mão, para mostrar o que a instrução faz
    const auto manual = ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
                        ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
    fmt::print("\n      a versão à mão dá o mesmo? {}\n", manual == trocado);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ Por que `memcpy` do struct inteiro está errado
// ─────────────────────────────────────────────────────────────────────────────
struct Pacote {
    std::uint16_t tipo;
    std::uint32_t tamanho;
    std::uint8_t  versao;
};

void demo_memcpy_errado() {
    fmt::print("\n── 5.4 ⚠ O erro de serializar o struct inteiro ──\n");

    fmt::print("      struct Pacote {{ uint16_t tipo; uint32_t tamanho; uint8_t versao; }};\n");
    fmt::print("        sizeof = {} bytes (os campos somam {} — o resto é PADDING)\n",
               sizeof(Pacote),
               sizeof(std::uint16_t) + sizeof(std::uint32_t) + sizeof(std::uint8_t));

    Pacote p{};
    p.tipo = 0x0102; p.tamanho = 0x03040506; p.versao = 0x07;

    const auto bruto = std::bit_cast<std::array<std::uint8_t, sizeof(Pacote)>>(p);
    fmt::print("\n      os bytes de `write(fd, &p, sizeof(p))`:\n        ");
    for (auto b : bruto) fmt::print("{:02X} ", b);
    fmt::print("\n");

    fmt::print("\n      Três problemas, todos silenciosos:\n");
    fmt::print("        1. PADDING — os bytes entre campos têm valor indefinido.\n");
    fmt::print("           Enviar padding vaza memória do processo (já foi CVE).\n");
    fmt::print("        2. ENDIANNESS — a ordem depende da arquitetura de quem\n");
    fmt::print("           escreveu, não do formato.\n");
    fmt::print("        3. LAYOUT — outro compilador pode ordenar ou alinhar\n");
    fmt::print("           diferente, mesmo na mesma arquitetura.\n");

    fmt::print("\n      Nenhum deles aparece em teste, porque quem escreve e quem\n");
    fmt::print("      lê é o MESMO binário. Aparece quando a outra ponta muda.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. A forma correta: serializar campo a campo, em ordem definida
// ─────────────────────────────────────────────────────────────────────────────
class Escritor {
public:
    explicit Escritor(std::vector<std::uint8_t>& saida) : saida_(saida) {}

    // Escreve em BIG-ENDIAN (network byte order), independente da máquina.
    // A ordem é definida por ESTE código, não pela arquitetura.
    template <typename T>
    void escrever(T v) {
        static_assert(std::is_unsigned_v<T>, "serialize só tipos sem sinal");
        for (int i = sizeof(T) - 1; i >= 0; --i) {
            saida_.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
        }
    }

private:
    std::vector<std::uint8_t>& saida_;
};

class Leitor {
public:
    explicit Leitor(std::span<const std::uint8_t> dados) : dados_(dados) {}

    template <typename T>
    T ler() {
        static_assert(std::is_unsigned_v<T>);
        T v = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            v = static_cast<T>((v << 8) | dados_[pos_++]);
        }
        return v;
    }

    std::size_t posicao() const { return pos_; }

private:
    std::span<const std::uint8_t> dados_;
    std::size_t                   pos_ = 0;
};

void demo_serializacao_correta() {
    fmt::print("\n── 5.5 A forma correta ──\n");

    const Pacote original{0x0102, 0x03040506, 0x07};

    std::vector<std::uint8_t> buffer;
    Escritor w{buffer};
    w.escrever(original.tipo);
    w.escrever(original.tamanho);
    w.escrever(original.versao);

    fmt::print("      serializado campo a campo, em big-endian:\n        ");
    for (auto b : buffer) fmt::print("{:02X} ", b);
    fmt::print("\n        → {} bytes, contra {} do struct cru\n",
               buffer.size(), sizeof(Pacote));
    fmt::print("        (sem padding, e a ordem é a do FORMATO)\n");

    Leitor r{buffer};
    Pacote lido{};
    lido.tipo    = r.ler<std::uint16_t>();
    lido.tamanho = r.ler<std::uint32_t>();
    lido.versao  = r.ler<std::uint8_t>();

    const bool ok = lido.tipo == original.tipo &&
                    lido.tamanho == original.tamanho &&
                    lido.versao == original.versao;
    fmt::print("\n      round-trip: tipo=0x{:04X} tamanho=0x{:08X} versao=0x{:02X} → {}\n",
               lido.tipo, lido.tamanho, lido.versao, ok ? "✓ idêntico" : "✗");

    fmt::print("\n      Por que esta versão é correta:\n");
    fmt::print("        • os shifts NÃO dependem do endianness da máquina —\n");
    fmt::print("          `v >> 24` pega o byte mais significativo em qualquer\n");
    fmt::print("          arquitetura, porque opera sobre o VALOR, não os bytes\n");
    fmt::print("        • nenhum padding é escrito\n");
    fmt::print("        • o formato está no CÓDIGO, e é o mesmo em todo lugar\n");

    fmt::print("\n      Este é o mecanismo por trás de Protobuf, MessagePack,\n");
    fmt::print("      Cap'n Proto e de todo protocolo binário sério.\n");
    fmt::print("      Ver proto/ e src/rabbitmq/ neste repositório.\n");

    fmt::print("\n      ⚠  e o detalhe que fecha: `v >> (i*8)` funciona porque\n");
    fmt::print("         deslocamento é uma operação sobre o VALOR NUMÉRICO.\n");
    fmt::print("         Endianness só existe quando você olha os BYTES.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_endian();
    demo_ver_bytes();
    demo_byteswap();
    demo_memcpy_errado();
    demo_serializacao_correta();
}

} // namespace demo_endianness
