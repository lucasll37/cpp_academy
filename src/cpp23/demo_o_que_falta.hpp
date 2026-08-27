// =============================================================================
//  src/cpp23/demo_o_que_falta.hpp
//
//  O que este GCC ainda não tem — e o que fazer enquanto isso
//  ──────────────────────────────────────────────────────────
//  Toda ausência aqui vem com um substituto que FUNCIONA hoje. O objetivo não
//  é lamentar: é dar o caminho para escrever código que já resolve o problema
//  e que migra sem dor quando o toolchain acompanhar.
//
//  Tópicos:
//  • std::print / println — e por que a fmt já resolve
//  • std::mdspan — e a alternativa com operator[](i,j)
//  • ranges::to<C>() — e a construção por iteradores
//  • views::enumerate — e o zip com iota
//  • std::generator — e a corrotina escrita à mão
//  • deducing this — e o y-combinator
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <array>
#include <cstddef>
#include <ranges>
#include <string>
#include <vector>
#include <version>

namespace demo_o_que_falta {

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::print / std::println
// ─────────────────────────────────────────────────────────────────────────────
void demo_print() {
    fmt::print("\n── 5.1 std::print (ausente) ──\n");

    fmt::print("      O que o C++23 traria:\n");
    fmt::print("        std::print(\"x = {{}}\\n\", 42);\n");
    fmt::print("        std::println(\"x = {{}}\", 42);   // com \\n automático\n");

    fmt::print("\n      O substituto, que é literalmente a mesma coisa:\n");
    fmt::print("        fmt::print(\"x = {{}}\\n\", 42);\n");
    fmt::print("        fmt::println(\"x = {{}}\", 42);\n");
    fmt::println("        → e este demo usa fmt::println agora: x = {}", 42);

    fmt::print("\n      `std::print` NASCEU da `fmt` — a proposta P2093 foi\n");
    fmt::print("      escrita pelo autor da biblioteca. A API é idêntica de\n");
    fmt::print("      propósito, e a migração é trocar `fmt::` por `std::`.\n");

    fmt::print("\n      Este repositório usa `fmt` em TODOS os subprojetos por\n");
    fmt::print("      isso: quando o `std::print` chegar, a troca é mecânica.\n");
    fmt::print("      Ver src/strings/demo_format.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::mdspan
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t L, std::size_t C>
class MatrizSimples {
public:
    constexpr T&       operator[](std::size_t i, std::size_t j)       { return d_[i * C + j]; }
    constexpr const T& operator[](std::size_t i, std::size_t j) const { return d_[i * C + j]; }
    static constexpr std::size_t linhas()  { return L; }
    static constexpr std::size_t colunas() { return C; }
private:
    std::array<T, L * C> d_{};
};

void demo_mdspan() {
    fmt::print("\n── 5.2 std::mdspan (ausente) ──\n");

    fmt::print("      O que o C++23 traria: uma VISÃO multidimensional sobre\n");
    fmt::print("      memória contígua que já existe — o `span` de N dimensões.\n");
    fmt::print("        std::vector<double> dados(12);\n");
    fmt::print("        std::mdspan m{{dados.data(), 3, 4}};\n");
    fmt::print("        m[1, 2] = 5.0;\n");

    fmt::print("\n      O substituto para o caso em que você é DONO dos dados:\n");
    MatrizSimples<int, 2, 3> m;
    m[0, 0] = 1; m[0, 1] = 2; m[0, 2] = 3;
    m[1, 0] = 4; m[1, 1] = 5; m[1, 2] = 6;

    fmt::print("        MatrizSimples<int,2,3> com operator[](i,j):\n");
    for (std::size_t i = 0; i < m.linhas(); ++i) {
        fmt::print("          ");
        for (std::size_t j = 0; j < m.colunas(); ++j) fmt::print("{:3}", m[i, j]);
        fmt::print("\n");
    }

    fmt::print("\n      O que o substituto NÃO dá:\n");
    fmt::print("        • VISÃO sobre memória alheia (o mdspan não possui)\n");
    fmt::print("        • layouts alternativos (column-major, strided)\n");
    fmt::print("        • dimensões dinâmicas misturadas com estáticas\n");
    fmt::print("        • submdspan (fatiar sem copiar)\n");

    fmt::print("\n      Se você precisa disso hoje: a implementação de\n");
    fmt::print("      referência do Kokkos (`kokkos/mdspan` no GitHub) é\n");
    fmt::print("      header-only e compila em C++17.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ranges::to<Container>()
// ─────────────────────────────────────────────────────────────────────────────
void demo_ranges_to() {
    fmt::print("\n── 5.3 ranges::to<C>() (ausente) ──\n");

    const std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    auto pares_dobrados = v | std::views::filter([](int x) { return x % 2 == 0; })
                            | std::views::transform([](int x) { return x * 2; });

    fmt::print("      O que o C++23 traria:\n");
    fmt::print("        auto r = v | views::filter(par) | views::transform(x2)\n");
    fmt::print("                   | std::ranges::to<std::vector>();\n");

    fmt::print("\n      O substituto de hoje — construção por iteradores:\n");
    const std::vector<int> materializado(pares_dobrados.begin(), pares_dobrados.end());
    fmt::print("        std::vector<int> r(view.begin(), view.end());\n");
    fmt::print("        → {}\n", fmt::join(materializado, " "));

    fmt::print("\n      ⚠  o substituto só funciona se a view for um\n");
    fmt::print("         `common_range` (begin e end do mesmo tipo). Views com\n");
    fmt::print("         SENTINELA precisam de `views::common` antes:\n");
    fmt::print("           auto c = view | std::views::common;\n");
    fmt::print("           std::vector<int> r(c.begin(), c.end());\n");

    // A alternativa genérica que funciona hoje, e migra fácil depois
    const auto para_vetor = []<std::ranges::range R>(R&& r) {
        using T = std::ranges::range_value_t<R>;
        std::vector<T> saida;
        if constexpr (std::ranges::sized_range<R>) saida.reserve(std::ranges::size(r));
        for (auto&& x : r) saida.push_back(static_cast<T>(x));
        return saida;
    };

    fmt::print("\n      Ou um helper genérico de 6 linhas:\n");
    fmt::print("        para_vetor(view) → {}\n", fmt::join(para_vetor(pares_dobrados), " "));
    fmt::print("      Ele funciona com QUALQUER range, inclusive os que têm\n");
    fmt::print("      sentinela — e a migração para `ranges::to` é um sed.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. views::enumerate
// ─────────────────────────────────────────────────────────────────────────────
void demo_enumerate() {
    fmt::print("\n── 5.4 views::enumerate (ausente) ──\n");

    const std::vector<std::string> nomes{"Ada", "Grace", "Alan"};

    fmt::print("      O que o C++23 traria:\n");
    fmt::print("        for (auto [i, nome] : nomes | views::enumerate)\n");

    fmt::print("\n      O substituto, com o que ESTE GCC tem:\n");
    fmt::print("        for (auto [i, nome] : views::zip(views::iota(0uz), nomes))\n");
    for (const auto& [i, nome] : std::views::zip(std::views::iota(0uz), nomes)) {
        fmt::print("          [{}] {}\n", i, nome);
    }

    fmt::print("\n      Funciona porque `views::iota(0uz)` é uma sequência\n");
    fmt::print("      INFINITA, e o `zip` para no menor range — ou seja, no\n");
    fmt::print("      tamanho de `nomes`.\n");
    fmt::print("      (o `0uz` é o sufixo de size_t do C++23 — ver 2.4)\n");

    fmt::print("\n      A diferença real para o `enumerate`: nenhuma, exceto a\n");
    fmt::print("      legibilidade. Vale definir um alias:\n");
    fmt::print("        inline constexpr auto enumerar = []<std::ranges::range R>(R&& r) {{\n");
    fmt::print("            return std::views::zip(std::views::iota(0uz), r);\n");
    fmt::print("        }};\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. std::generator e deducing this
// ─────────────────────────────────────────────────────────────────────────────
void demo_resto() {
    fmt::print("\n── 5.5 std::generator e deducing this (ausentes) ──\n");

    fmt::print("      std::generator<T> — a corrotina geradora pronta:\n");
    fmt::print("        std::generator<int> contar(int n) {{\n");
    fmt::print("            for (int i = 0; i < n; ++i) co_yield i;\n");
    fmt::print("        }}\n");
    fmt::print("      Substituto: escrever o `promise_type` à mão. Este\n");
    fmt::print("      repositório já tem um em src/coroutines/demo_generator.hpp —\n");
    fmt::print("      são ~40 linhas, e ensinam como o `generator` funciona.\n");

    fmt::print("\n      deducing this (`this auto&& self`):\n");
    fmt::print("        resolveria recursão em lambda e CRTP sem template.\n");
    fmt::print("      Substitutos:\n");
    fmt::print("        recursão em lambda → y-combinator\n");
    fmt::print("                             (src/lambdas/demo_padroes.hpp, 5.3)\n");
    fmt::print("        CRTP               → o CRTP clássico\n");
    fmt::print("                             (src/templates/demo_crtp.hpp)\n");
    fmt::print("        const/non-const    → o truque do const_cast\n");
    fmt::print("                             (src/casts/demo_const_cast.hpp, 3.2)\n");

    fmt::print("\n      Também ausentes, e menos sentidos:\n");
    fmt::print("        std::flat_map / flat_set     → use vector ordenado\n");
    fmt::print("                                       (src/compile_time/, 5.3)\n");
    fmt::print("        std::out_ptr / inout_ptr     → o idioma T* temporário\n");
    fmt::print("        ranges::starts_with          → std::equal\n");
    fmt::print("        import std;                  → #include\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. A régua para decidir
// ─────────────────────────────────────────────────────────────────────────────
void demo_decidir() {
    fmt::print("\n── 5.6 Quando adotar uma feature nova ──\n");

    fmt::print("      A pergunta não é \"o C++23 tem?\", é \"o meu toolchain\n");
    fmt::print("      MÍNIMO tem?\". As três perguntas, em ordem:\n\n");

    fmt::print("        1. Qual é o compilador MAIS ANTIGO que precisa compilar\n");
    fmt::print("           este código? (CI, distribuição LTS, cliente)\n");
    fmt::print("        2. A feature-test macro está definida nele?\n");
    fmt::print("        3. Ela LINKA sem flags extras?\n");

    fmt::print("\n      Se as três respostas forem sim, use direto.\n");
    fmt::print("      Se a 2 ou a 3 falhar, o padrão é o de 1.4:\n");
    fmt::print("        #if defined(__cpp_lib_X) && __cpp_lib_X >= AAAAMM\n");
    fmt::print("            // caminho novo\n");
    fmt::print("        #else\n");
    fmt::print("            // fallback equivalente\n");
    fmt::print("        #endif\n");

    fmt::print("\n      E a regra que este subprojeto inteiro demonstra:\n");
    fmt::print("      MEÇA. A tabela de suporte em src/ROADMAP.md tinha SEIS\n");
    fmt::print("      entradas erradas antes de o demo_inventario.hpp existir —\n");
    fmt::print("      todas na direção de subestimar o que já estava disponível.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_print();
    demo_mdspan();
    demo_ranges_to();
    demo_enumerate();
    demo_resto();
    demo_decidir();
}

} // namespace demo_o_que_falta
