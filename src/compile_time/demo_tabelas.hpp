// =============================================================================
//  src/compile_time/demo_tabelas.hpp
//
//  Gerando dados na compilação — o pagamento da técnica
//  ────────────────────────────────────────────────────
//  Os demos anteriores mostraram o MECANISMO. Este mostra para que serve:
//  substituir trabalho de inicialização por dados que já nascem prontos no
//  binário.
//
//  O ganho tem duas faces:
//    • runtime — a tabela não é calculada; ela já está na seção .rodata
//    • correção — o cálculo aconteceu sob as regras do constexpr, onde
//      qualquer UB é erro de compilação
//
//  Tópicos:
//  • Tabela de lookup gerada na compilação
//  • Onde a tabela realmente vai parar no binário (verificado com nm/objdump)
//  • Ordenação em compilação — e um mapa constexpr
//  • Quando NÃO vale a pena
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

namespace demo_tabelas {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Tabela de lookup — CRC-32
//
//     O clássico: a tabela de 256 entradas do CRC-32 costuma ser gerada na
//     inicialização do programa, ou colada à mão num arquivo gerado. Com
//     constexpr, ela é calculada pelo compilador e gravada no binário.
// ─────────────────────────────────────────────────────────────────────────────
constexpr std::array<std::uint32_t, 256> gerar_tabela_crc32() {
    constexpr std::uint32_t polinomio = 0xEDB88320u;
    std::array<std::uint32_t, 256> tabela{};

    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) ? (polinomio ^ (c >> 1)) : (c >> 1);
        }
        tabela[i] = c;
    }
    return tabela;
}

// `inline constexpr` numa variável de header: uma única instância no programa,
// sem violar a ODR. Antes do C++17 isso exigiria um .cpp.
inline constexpr auto TABELA_CRC32 = gerar_tabela_crc32();

constexpr std::uint32_t crc32(std::string_view dados) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (char ch : dados) {
        c = TABELA_CRC32[(c ^ static_cast<std::uint8_t>(ch)) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

void demo_lookup() {
    fmt::print("\n── 5.1 Tabela de lookup gerada na compilação ──\n");

    // Os 256 valores foram calculados pelo COMPILADOR
    static_assert(TABELA_CRC32[0]   == 0x00000000u);
    static_assert(TABELA_CRC32[1]   == 0x77073096u);
    static_assert(TABELA_CRC32[255] == 0x2D02EF8Du);

    fmt::print("      TABELA_CRC32 tem {} entradas, calculadas na compilação\n",
               TABELA_CRC32.size());
    fmt::print("        [0]   = 0x{:08X}\n", TABELA_CRC32[0]);
    fmt::print("        [1]   = 0x{:08X}\n", TABELA_CRC32[1]);
    fmt::print("        [255] = 0x{:08X}\n", TABELA_CRC32[255]);

    // E o próprio CRC de uma string literal também é constante
    constexpr auto crc_literal = crc32("cpp_academy");
    static_assert(crc_literal != 0);
    fmt::print("      crc32(\"cpp_academy\") = 0x{:08X}  (calculado na compilação)\n",
               crc_literal);

    // Com dado de runtime, a MESMA função roda normalmente
    std::string_view entrada = "cpp_academy";
    fmt::print("      crc32(variável)      = 0x{:08X}  (calculado na execução)\n",
               crc32(entrada));

    fmt::print("\n      A tabela ocupa {} bytes, e eles estão no BINÁRIO —\n",
               TABELA_CRC32.size() * sizeof(std::uint32_t));
    fmt::print("      não há laço de inicialização ao iniciar o programa.\n");
    fmt::print("      Confira você mesmo (o -C demangla os nomes, e o ' O '\n");
    fmt::print("      filtra só os símbolos de DADOS):\n");
    fmt::print("        objdump -tC dist/bin/compile_time \\\n");
    fmt::print("          | grep ' O ' | grep -E 'TABELA|PRIORIDADES'\n\n");
    fmt::print("      A saída real, e a nuance que ela revela:\n");
    fmt::print("        ...  O .rodata      0x400  demo_tabelas::TABELA_CRC32\n");
    fmt::print("        ...  O .data.rel.ro 0x0d8  demo_tabelas::TABELA_HTTP\n");
    fmt::print("        ...  O .data.rel.ro 0x078  demo_tabelas::PRIORIDADES\n\n");
    fmt::print("      Por que seções diferentes? A TABELA_CRC32 é só uint32_t —\n");
    fmt::print("      valores puros, vão direto para .rodata. A TABELA_HTTP e o\n");
    fmt::print("      mapa PRIORIDADES contêm `string_view`, que são PONTEIROS: o\n");
    fmt::print("      endereço final só é conhecido quando o loader mapeia o\n");
    fmt::print("      binário, então vão para .data.rel.ro — \"read-only APÓS\n");
    fmt::print("      relocação\". As três são constantes; nenhuma está em .bss\n");
    fmt::print("      (a seção dos dados zerados, que é o que indicaria\n");
    fmt::print("      inicialização em runtime).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Ordenação em compilação
//
//     Desde o C++20, `std::sort` é constexpr. Isso permite construir estruturas
//     ORDENADAS na compilação, e depois usar busca binária em runtime — sem
//     nunca pagar pela ordenação.
// ─────────────────────────────────────────────────────────────────────────────
struct EntradaHttp {
    int              codigo;
    std::string_view descricao;

    constexpr bool operator<(const EntradaHttp& o) const { return codigo < o.codigo; }
};

// A tabela é escrita fora de ordem de propósito — o compilador ordena.
constexpr auto gerar_tabela_http() {
    std::array<EntradaHttp, 9> t{{
        {500, "Internal Server Error"},
        {200, "OK"},
        {404, "Not Found"},
        {301, "Moved Permanently"},
        {403, "Forbidden"},
        {201, "Created"},
        {503, "Service Unavailable"},
        {400, "Bad Request"},
        {204, "No Content"},
    }};
    std::sort(t.begin(), t.end());          // constexpr desde C++20
    return t;
}

inline constexpr auto TABELA_HTTP = gerar_tabela_http();

// Busca binária — válida porque a tabela está PROVADAMENTE ordenada
constexpr std::string_view descrever_http(int codigo) {
    const auto it = std::lower_bound(TABELA_HTTP.begin(), TABELA_HTTP.end(), codigo,
                                     [](const EntradaHttp& e, int c) { return e.codigo < c; });
    if (it == TABELA_HTTP.end() || it->codigo != codigo) return "Unknown";
    return it->descricao;
}

void demo_ordenacao() {
    fmt::print("\n── 5.2 Ordenação em compilação ──\n");

    // A prova de que ordenou: um static_assert sobre a ordem
    static_assert(std::is_sorted(TABELA_HTTP.begin(), TABELA_HTTP.end()));
    static_assert(TABELA_HTTP.front().codigo == 200);
    static_assert(TABELA_HTTP.back().codigo  == 503);

    fmt::print("      tabela escrita fora de ordem, ordenada pelo COMPILADOR:\n        ");
    for (const auto& e : TABELA_HTTP) fmt::print("{} ", e.codigo);
    fmt::print("\n");

    static_assert(descrever_http(404) == "Not Found");
    static_assert(descrever_http(999) == "Unknown");

    for (int c : {200, 404, 503, 999}) {
        fmt::print("      descrever_http({}) = {}\n", c, descrever_http(c));
    }

    fmt::print("\n      A busca binária em runtime é válida porque um\n");
    fmt::print("      `static_assert(is_sorted(...))` PROVOU a ordenação na\n");
    fmt::print("      compilação. Sem isso, a busca binária sobre dados não\n");
    fmt::print("      ordenados seria UB silencioso (ver src/stl/demo_algorithms).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Um "mapa" constexpr — sem std::map, sem alocação
//
//     `std::map` não é constexpr (aloca nós). Para um mapeamento pequeno e
//     fixo, um array ordenado + busca binária é mais rápido E cabe na
//     compilação.
// ─────────────────────────────────────────────────────────────────────────────
template <typename Chave, typename Valor, std::size_t N>
class MapaFixo {
public:
    constexpr explicit MapaFixo(std::array<std::pair<Chave, Valor>, N> dados)
        : dados_(dados) {
        std::sort(dados_.begin(), dados_.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
    }

    // Devolve o ÍNDICE, ou N se não achou.
    //
    // Por que índice e não ponteiro: `return p ? *p : padrao;` parece natural,
    // mas a conversão de um ponteiro-para-membro-de-objeto-constexpr em bool
    // não é uma expressão constante em todos os modos do GCC — o mesmo código
    // compila em -O2 e falha sob -fsanitize. Aritmética de índice é sempre
    // constante, e por isso é a base mais robusta para uma API constexpr.
    constexpr std::size_t indice_de(const Chave& k) const {
        const auto it = std::lower_bound(dados_.begin(), dados_.end(), k,
                                         [](const auto& p, const Chave& c) { return p.first < c; });
        if (it == dados_.end() || it->first != k) return N;
        return static_cast<std::size_t>(it - dados_.begin());
    }

    constexpr bool contem(const Chave& k) const { return indice_de(k) != N; }

    constexpr Valor valor_ou(const Chave& k, Valor padrao) const {
        const std::size_t i = indice_de(k);
        return i == N ? padrao : dados_[i].second;
    }

    constexpr std::size_t tamanho() const { return N; }

private:
    std::array<std::pair<Chave, Valor>, N> dados_;
};

inline constexpr MapaFixo<std::string_view, int, 5> PRIORIDADES{{{
    {"critico", 0}, {"erro", 1}, {"aviso", 2}, {"info", 3}, {"debug", 4},
}}};

void demo_mapa() {
    fmt::print("\n── 5.3 Um mapa constexpr ──\n");

    static_assert(PRIORIDADES.valor_ou("erro", -1)  == 1);
    static_assert(PRIORIDADES.valor_ou("debug", -1) == 4);
    static_assert(PRIORIDADES.valor_ou("nada", -1)  == -1);

    for (std::string_view nivel : {"critico", "aviso", "debug", "inexistente"}) {
        fmt::print("      PRIORIDADES[\"{}\"] = {}\n", nivel,
                   PRIORIDADES.valor_ou(nivel, -1));
    }

    fmt::print("\n      | estrutura          | constexpr | aloca | busca    |\n");
    fmt::print("      |--------------------|-----------|-------|----------|\n");
    fmt::print("      | std::map           | não       | sim   | O(log n) |\n");
    fmt::print("      | std::unordered_map | não       | sim   | O(1) méd.|\n");
    fmt::print("      | array ordenado     | SIM       | não   | O(log n) |\n");
    fmt::print("      Para até algumas dezenas de entradas fixas, o array\n");
    fmt::print("      ordenado ganha nas três colunas — inclusive em velocidade,\n");
    fmt::print("      porque é contíguo (ver src/stl/demo_containers, 1.7).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Quando NÃO vale a pena
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_nao() {
    fmt::print("\n── 5.4 Quando não vale a pena ──\n");

    fmt::print("      O custo da avaliação em compilação é pago pelo COMPILADOR,\n");
    fmt::print("      e não é barato: o interpretador de constexpr do GCC é\n");
    fmt::print("      ordens de grandeza mais lento que o código nativo.\n");

    fmt::print("\n      Não vale a pena quando:\n");
    fmt::print("        • a tabela é grande e o build fica lento\n");
    fmt::print("          (o teto do GCC é -fconstexpr-ops-limit, 33.5M passos)\n");
    fmt::print("        • os dados mudam com frequência — um arquivo de dados\n");
    fmt::print("          lido em runtime é mais fácil de manter\n");
    fmt::print("        • o cálculo já era trivial no startup\n");
    fmt::print("        • a tabela é grande e infla o binário sem ganho real\n");

    fmt::print("\n      Vale muito a pena quando:\n");
    fmt::print("        • a tabela é pequena/média e imutável (CRC, seno, códigos)\n");
    fmt::print("        • você quer PROVAR uma propriedade (ordenação, ausência\n");
    fmt::print("          de UB) e não só testá-la\n");
    fmt::print("        • está em ambiente sem heap (embarcado, kernel)\n");
    fmt::print("        • quer eliminar inicialização estática dinâmica\n");
    fmt::print("          (ver `constinit` em demo_consteval.hpp)\n");

    fmt::print("\n      Medida honesta: compile este subprojeto com\n");
    fmt::print("        time g++ -std=c++23 -c src/compile_time/main.cpp\n");
    fmt::print("      e compare com um alvo sem constexpr pesado. O tempo de\n");
    fmt::print("      build É o custo, e ele é real.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_lookup();
    demo_ordenacao();
    demo_mapa();
    demo_quando_nao();
}

} // namespace demo_tabelas
