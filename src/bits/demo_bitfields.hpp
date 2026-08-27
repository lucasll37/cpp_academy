// =============================================================================
//  src/bits/demo_bitfields.hpp
//
//  Campos de bits — a sintaxe nativa, e por que ela decepciona
//  ───────────────────────────────────────────────────────────
//  C++ tem sintaxe nativa para empacotar campos num inteiro:
//
//      struct Pacote {
//          unsigned versao  : 4;
//          unsigned tipo    : 4;
//          unsigned tamanho : 24;
//      };
//
//  Parece a solução ideal para protocolos binários e registradores. Não é —
//  porque quase TUDO sobre o layout é "definido pela implementação":
//
//    • a ORDEM dos campos dentro da palavra
//    • se um campo pode ATRAVESSAR a fronteira da unidade de alocação
//    • o alinhamento e o tamanho total
//    • se `int x : 1` guarda 0/1 ou 0/-1
//
//  Este demo mostra a sintaxe, MEDE o layout real, e explica quando usar
//  máscaras explícitas (demo_operadores.hpp, 1.5) em vez disso.
//
//  Tópicos:
//  • A sintaxe e o que ela economiza
//  • ⚠ O layout medido — e o que o padrão NÃO garante
//  • ⚠ A armadilha do campo signed de 1 bit
//  • Campos anônimos e o `:0` de alinhamento
//  • Quando usar, e quando usar máscaras
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace demo_bitfields {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A sintaxe
// ─────────────────────────────────────────────────────────────────────────────
struct CabecalhoIP {
    std::uint8_t versao : 4;      // 4 bits
    std::uint8_t ihl    : 4;      // 4 bits  → juntos, 1 byte
    std::uint8_t dscp   : 6;
    std::uint8_t ecn    : 2;      // → mais 1 byte
    std::uint16_t comprimento;    // campo normal, 2 bytes
};

struct SemCampos {
    std::uint8_t  versao_ihl;     // os dois empacotados à mão
    std::uint8_t  dscp_ecn;
    std::uint16_t comprimento;
};

void demo_sintaxe() {
    fmt::print("\n── 4.1 A sintaxe ──\n");

    CabecalhoIP h{};
    h.versao = 4;
    h.ihl    = 5;
    h.dscp   = 0;
    h.ecn    = 0;
    h.comprimento = 1500;

    // ⚠ O `+` unário na frente de cada campo NÃO é decoração: `fmt::print`
    //   recebe os argumentos por REFERÊNCIA, e não existe referência para um
    //   campo de bits — ele não tem endereço próprio. O `+` promove o campo a
    //   um `int` temporário, que tem. Sem ele:
    //     error: cannot bind bit-field 'h.versao' to 'unsigned char&'
    fmt::print("      CabecalhoIP: versao={} ihl={} dscp={} ecn={} comprimento={}\n",
               +h.versao, +h.ihl, +h.dscp, +h.ecn, h.comprimento);

    fmt::print("\n      sizeof(CabecalhoIP) = {} bytes\n", sizeof(CabecalhoIP));
    fmt::print("      sizeof(SemCampos)   = {} bytes  (empacotado à mão)\n",
               sizeof(SemCampos));

    fmt::print("\n      O que a sintaxe economiza: os shifts e as máscaras.\n");
    fmt::print("        com campo : h.versao = 4;\n");
    fmt::print("        sem campo : b = (b & 0x0F) | (4 << 4);\n");

    // ⚠ A atribuição TRUNCA silenciosamente
    // O GCC pega este caso específico com -Woverflow (que vem em -Wall), porque
    // o valor é um LITERAL. Suprimimos porque o truncamento é a demonstração.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
    h.versao = 20;                            // 20 = 0b10100, mas só cabem 4 bits
#pragma GCC diagnostic pop
    fmt::print("\n      ⚠  h.versao = 20 (0b10100) num campo de 4 bits → {}\n", +h.versao);
    fmt::print("         Truncou para os 4 bits de baixo.\n");
    fmt::print("         O GCC avisou aqui (-Woverflow) porque 20 é um LITERAL.\n");
    fmt::print("         Com um valor de RUNTIME, o truncamento é silencioso:\n");
    fmt::print("           uint8_t v = ler_do_arquivo();\n");
    fmt::print("           h.versao = v;      // sem warning, trunca igual\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. ⚠ O layout medido — e o que o padrão não garante
// ─────────────────────────────────────────────────────────────────────────────
struct Layout {
    std::uint32_t a : 4;
    std::uint32_t b : 4;
    std::uint32_t c : 8;
    std::uint32_t d : 16;
};

void demo_layout() {
    fmt::print("\n── 4.2 ⚠ O layout medido ──\n");

    Layout v{};
    v.a = 0xF; v.b = 0x0; v.c = 0x00; v.d = 0x0000;

    std::uint32_t bruto = 0;
    std::memcpy(&bruto, &v, sizeof(bruto));

    fmt::print("      struct {{ uint32_t a:4, b:4, c:8, d:16; }}\n");
    fmt::print("      sizeof = {} bytes\n\n", sizeof(Layout));

    const auto mostrar = [](const char* nome, Layout x) {
        std::uint32_t r = 0;
        std::memcpy(&r, &x, sizeof(r));
        fmt::print("        {:<12} → 0x{:08X}  {}\n", nome, r,
                   std::bitset<32>{r}.to_string());
    };

    mostrar("só a=0xF",  Layout{0xF, 0, 0, 0});
    mostrar("só b=0xF",  Layout{0, 0xF, 0, 0});
    mostrar("só c=0xFF", Layout{0, 0, 0xFF, 0});
    mostrar("só d=0xFFFF", Layout{0, 0, 0, 0xFFFF});

    fmt::print("\n      Neste compilador (GCC/x86-64, little-endian), o campo\n");
    fmt::print("      `a` ficou nos bits MENOS significativos.\n");

    fmt::print("\n      ⚠  MAS O PADRÃO NÃO GARANTE ISSO. São \"implementation-\n");
    fmt::print("         defined\":\n");
    fmt::print("          • a ordem (o primeiro campo pode ir no bit mais alto)\n");
    fmt::print("          • se um campo atravessa a fronteira da palavra\n");
    fmt::print("          • o alinhamento e o padding\n");
    fmt::print("          • o tipo base permitido (só int/unsigned no C++ antigo)\n");

    fmt::print("\n      Consequência prática: um struct com campos de bits\n");
    fmt::print("      NÃO é portável entre compiladores nem entre arquiteturas.\n");
    fmt::print("      Escrever um cabeçalho de protocolo assim e enviá-lo pela\n");
    fmt::print("      rede é um bug esperando o dia em que a outra ponta usa\n");
    fmt::print("      outro compilador.\n");

    fmt::print("\n      A alternativa PORTÁVEL é a de demo_operadores.hpp, 1.5:\n");
    fmt::print("        constexpr CampoBits VERSAO{{24, 8}};\n");
    fmt::print("        reg = VERSAO.inserir(reg, 2);\n");
    fmt::print("      Mais verbosa, e o layout é VOCÊ quem define.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ O campo signed de 1 bit
//
//     `int x : 1` tem faixa {-1, 0}, não {0, 1} — porque o único bit É o bit
//     de sinal. Atribuir 1 e ler -1 é o comportamento correto e surpreendente.
// ─────────────────────────────────────────────────────────────────────────────
struct ComSinal {
    int          com_sinal : 1;
    unsigned int sem_sinal : 1;
};

void demo_signed() {
    fmt::print("\n── 4.3 ⚠ O campo signed de 1 bit ──\n");

    ComSinal v{};
    // O GCC AVISA aqui: "overflow in conversion from 'int' to 'signed char:1'
    // changes value from '1' to '-1'". O aviso É a lição desta seção.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
    v.com_sinal = 1;
#pragma GCC diagnostic pop
    v.sem_sinal = 1;

    fmt::print("      struct {{ int com_sinal:1; unsigned sem_sinal:1; }}\n");
    fmt::print("      após atribuir 1 aos dois:\n");
    fmt::print("        com_sinal = {}   ← o único bit É o bit de SINAL\n", +v.com_sinal);
    fmt::print("        sem_sinal = {}\n", +v.sem_sinal);
    fmt::print("      ↑ e o GCC AVISOU na atribuição:\n");
    fmt::print("        \"overflow in conversion from 'int' to 'signed char:1'\n");
    fmt::print("         changes value from '1' to '-1'\"\n");

    fmt::print("\n      O bug clássico:\n");
    fmt::print("        struct Flags {{ int ativo : 1; }};\n");
    fmt::print("        f.ativo = 1;\n");
    fmt::print("        if (f.ativo == 1) ...    // FALSO! f.ativo é -1\n");
    fmt::print("        if (f.ativo)      ...    // ✓ verdadeiro (-1 é truthy)\n");

    fmt::print("\n      Regra: campos de bits são SEMPRE `unsigned`, ou `bool`.\n");
    fmt::print("        struct Flags {{ bool ativo : 1; }};   // ✓ C++ permite bool\n");
    fmt::print("      E note que `char x : 1` tem o mesmo problema, porque a\n");
    fmt::print("      assinatura de `char` também é definida pela implementação.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Campos anônimos e o `:0`
// ─────────────────────────────────────────────────────────────────────────────
struct ComPreenchimento {
    unsigned a : 3;
    unsigned   : 5;      // anônimo: 5 bits de preenchimento, sem nome
    unsigned b : 4;
};

struct ComAlinhamento {
    unsigned a : 3;
    unsigned   : 0;      // ":0" força o PRÓXIMO campo a começar numa nova unidade
    unsigned b : 4;
};

void demo_anonimos() {
    fmt::print("\n── 4.4 Campos anônimos e `:0` ──\n");

    fmt::print("      unsigned : 5;   → 5 bits de PREENCHIMENTO, sem nome\n");
    fmt::print("      unsigned : 0;   → força o próximo campo a começar numa\n");
    fmt::print("                        nova unidade de alocação\n\n");

    fmt::print("      sizeof(ComPreenchimento) = {} bytes\n", sizeof(ComPreenchimento));
    fmt::print("      sizeof(ComAlinhamento)   = {} bytes\n", sizeof(ComAlinhamento));

    ComPreenchimento p{}; p.a = 7; p.b = 0xF;
    ComAlinhamento   c{}; c.a = 7; c.b = 0xF;

    std::uint32_t bp = 0, bc = 0;
    std::memcpy(&bp, &p, sizeof(bp));
    std::memcpy(&bc, &c, sizeof(bc));

    fmt::print("\n      com a=7, b=0xF:\n");
    fmt::print("        preenchimento : {}\n", std::bitset<32>{bp}.to_string());
    fmt::print("        alinhamento   : {}\n", std::bitset<32>{bc}.to_string());
    fmt::print("      ↑ o `:0` empurrou `b` para a próxima unidade de 32 bits.\n");

    fmt::print("\n      Os campos anônimos servem para MAPEAR um layout que\n");
    fmt::print("      já existe (um registrador de hardware com bits reservados),\n");
    fmt::print("      documentando os buracos em vez de escondê-los.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Quando usar
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando() {
    fmt::print("\n── 4.5 Quando usar campos de bits ──\n");

    fmt::print("      ✓ USE quando:\n");
    fmt::print("        • o objetivo é ECONOMIZAR MEMÓRIA num struct que existe\n");
    fmt::print("          aos milhões (nós de árvore, entidades de um ECS)\n");
    fmt::print("        • o struct é INTERNO ao seu programa e nunca sai dele\n");
    fmt::print("        • a legibilidade de `x.campo` compensa\n");

    fmt::print("\n      ✗ NÃO USE quando:\n");
    fmt::print("        • os bytes vão para a REDE ou para um ARQUIVO\n");
    fmt::print("        • o struct mapeia um registrador de hardware\n");
    fmt::print("          (use máscaras explícitas + volatile)\n");
    fmt::print("        • outro compilador/arquitetura vai ler os mesmos bytes\n");
    fmt::print("        • você precisa do endereço ou de uma REFERÊNCIA a um campo\n");
    fmt::print("          (`&x.campo` e `T& r = x.campo` não compilam — um\n");
    fmt::print("           campo de bits não tem endereço próprio; foi por isso\n");
    fmt::print("           que a seção 4.1 precisou do `+` antes de cada campo)\n");

    fmt::print("\n      E há custos que não são óbvios:\n");
    fmt::print("        • ler/escrever um campo vira shift + máscara — mais\n");
    fmt::print("          instruções que acessar um byte inteiro\n");
    fmt::print("        • dois campos na mesma palavra NÃO podem ser acessados\n");
    fmt::print("          concorrentemente sem sincronização: escrever num deles\n");
    fmt::print("          faz read-modify-write da palavra INTEIRA\n");
    fmt::print("          (é data race — o C++11 definiu isso explicitamente)\n");

    fmt::print("\n      | objetivo                  | ferramenta              |\n");
    fmt::print("      |---------------------------|-------------------------|\n");
    fmt::print("      | economizar memória interna| campos de bits          |\n");
    fmt::print("      | protocolo binário/arquivo | máscaras + shift (1.5)  |\n");
    fmt::print("      | registrador de hardware   | máscaras + volatile     |\n");
    fmt::print("      | conjunto de flags nomeadas| enum class (3.4)        |\n");
    fmt::print("      | muitos bits homogêneos    | std::bitset (3.x)       |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_sintaxe();
    demo_layout();
    demo_signed();
    demo_anonimos();
    demo_quando();
}

} // namespace demo_bitfields
