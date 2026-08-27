// =============================================================================
//  src/ranges/demo_pipelines.hpp
//
//  Pipelines, composição e a armadilha do tempo de vida
//  ────────────────────────────────────────────────────
//  O `operator|` não é mágica: cada adaptador é um objeto invocável que recebe
//  um range e devolve outro. `a | b | c` é só `c(b(a))` escrito da esquerda
//  para a direita — a ordem em que a gente LÊ o que acontece.
//
//  A parte que morde é o tempo de vida. Uma view referencia sua origem. Se a
//  origem for um temporário, a view fica pendurada — e o compilador só te
//  protege de alguns casos.
//
//  Tópicos:
//  • Como o operator| funciona por dentro
//  • Adaptadores parcialmente aplicados e reutilizáveis
//  • borrowed_range: quando é seguro devolver uma view
//  • ⚠ A view pendurada — e o que o compilador pega
//  • owning_view e views::all
//  • Custo de composição: zero
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <ranges>
#include <memory>
#include <span>
#include <type_traits>
#include <string>
#include <string_view>
#include <vector>

namespace demo_pipelines {

namespace vw = std::views;
namespace rg = std::ranges;

// ─────────────────────────────────────────────────────────────────────────────
//  1. O operator| por dentro
//
//     `vw::filter(pred)` sozinho NÃO é uma view — é um adaptador parcialmente
//     aplicado, um functor esperando o range. Guardá-lo numa variável e
//     reutilizá-lo é perfeitamente válido.
// ─────────────────────────────────────────────────────────────────────────────
void demo_operator_pipe() {
    fmt::print("\n── 2.1 Como o operator| funciona ──\n");

    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // As três formas são EQUIVALENTES:
    auto forma_pipe    = v | vw::filter([](int x) { return x % 2 == 0; });
    auto forma_chamada = vw::filter([](int x) { return x % 2 == 0; })(v);
    auto forma_direta  = rg::filter_view(v, [](int x) { return x % 2 == 0; });

    fmt::print("      v | vw::filter(p)          → {}\n", fmt::join(forma_pipe, " "));
    fmt::print("      vw::filter(p)(v)           → {}\n", fmt::join(forma_chamada, " "));
    fmt::print("      rg::filter_view(v, p)      → {}\n", fmt::join(forma_direta, " "));

    fmt::print("      `a | b | c` é literalmente `c(b(a))`, só que legível na\n");
    fmt::print("      ordem em que as coisas acontecem.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Adaptadores reutilizáveis
//
//     Como o adaptador é um objeto, ele pode ser nomeado, guardado, passado
//     como argumento e COMPOSTO com outro adaptador — antes de ver um range.
// ─────────────────────────────────────────────────────────────────────────────
void demo_reutilizaveis() {
    fmt::print("\n── 2.2 Adaptadores nomeados e compostos ──\n");

    // Um adaptador guardado numa variável
    auto so_pares = vw::filter([](int x) { return x % 2 == 0; });
    auto dobrar   = vw::transform([](int x) { return x * 2; });

    // E a composição de dois adaptadores, ainda sem nenhum range à vista
    auto pares_dobrados = so_pares | dobrar;

    std::vector<int> a{1, 2, 3, 4, 5};
    std::vector<int> b{10, 11, 12, 13};

    fmt::print("      a | pares_dobrados         → {}\n", fmt::join(a | pares_dobrados, " "));
    fmt::print("      b | pares_dobrados         → {}\n", fmt::join(b | pares_dobrados, " "));

    fmt::print("      O mesmo pipeline aplicado a duas origens diferentes.\n");
    fmt::print("      Isto é o que os algoritmos clássicos não permitem: você\n");
    fmt::print("      não consegue guardar \"um copy_if seguido de transform\".\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ A view pendurada
//
//     Uma view referencia a origem. Se a origem é um TEMPORÁRIO que morre no
//     fim do statement, a view aponta para memória morta.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<int> produz_vetor() { return {1, 2, 3, 4, 5}; }

void demo_dangling() {
    fmt::print("\n── 2.3 ⚠ A view pendurada ──\n");

    // ✗ O erro:
    //     auto v = produz_vetor() | vw::filter(par);
    //     for (int x : v) ...        // ← o vetor já morreu
    //
    // Na prática, o C++20 SALVA este caso específico: como `produz_vetor()`
    // devolve um prvalue, o pipeline cria um `owning_view` que assume a posse
    // do vetor. O temporário é MOVIDO para dentro da view.
    auto segura = produz_vetor() | vw::filter([](int x) { return x % 2 == 0; });
    fmt::print("      produz_vetor() | filter    → {}  (owning_view assumiu a posse)\n",
               fmt::join(segura, " "));

    // ✗ Mas isto NÃO é salvo: aqui a origem é um LVALUE local que morre depois
    fmt::print("      O caso que continua perigoso:\n");
    fmt::print("        auto f() {{\n");
    fmt::print("            std::vector<int> local{{1,2,3}};\n");
    fmt::print("            return local | vw::filter(par);   // ← ref para `local`\n");
    fmt::print("        }}                                     // `local` morre aqui\n");
    fmt::print("      Devolver uma view de uma variável local é dangling.\n");

    // A biblioteca tem um conceito para isto: borrowed_range.
    // Um range é "borrowed" se seus iteradores continuam válidos mesmo depois
    // de o próprio objeto range morrer.
    fmt::print("\n      quem sobrevive à morte do objeto range (borrowed_range):\n");
    fmt::print("        std::vector<int>       : {}\n", rg::borrowed_range<std::vector<int>>);
    fmt::print("        std::vector<int>&      : {}\n", rg::borrowed_range<std::vector<int>&>);
    fmt::print("        std::string_view       : {}\n", rg::borrowed_range<std::string_view>);
    fmt::print("        std::span<int>         : {}  (por isso span é seguro devolver)\n",
               rg::borrowed_range<std::span<int>>);

    // Os algoritmos de ranges usam isso para te proteger: passar um temporário
    // devolve `std::ranges::dangling` em vez de um iterador utilizável.
    auto it = rg::find(produz_vetor(), 3);      // temporário → dangling
    fmt::print("\n      rg::find(produz_vetor(), 3) devolve std::ranges::dangling? {}\n",
               std::is_same_v<decltype(it), rg::dangling>);
    fmt::print("      *it nem COMPILA — o compilador te impede de usar o iterador.\n");
    fmt::print("      É a linguagem transformando um bug de runtime em erro de\n");
    fmt::print("      compilação. Os algoritmos clássicos não fazem isso.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. views::all, ref_view e owning_view
//
//     Todo pipeline começa convertendo a origem numa view. `views::all` é
//     quem faz isso, e a escolha depende da categoria de valor:
//       • lvalue  → ref_view    (referencia, O(1), não possui)
//       • prvalue → owning_view (assume a posse, move para dentro)
// ─────────────────────────────────────────────────────────────────────────────
void demo_all() {
    fmt::print("\n── 2.4 views::all — ref_view vs owning_view ──\n");

    std::vector<int> v{1, 2, 3};

    auto de_lvalue = vw::all(v);                 // → ref_view
    auto de_rvalue = vw::all(produz_vetor());    // → owning_view

    fmt::print("      vw::all(lvalue) é ref_view?    {}\n",
               std::is_same_v<decltype(de_lvalue), rg::ref_view<std::vector<int>>>);
    fmt::print("      vw::all(rvalue) é owning_view? {}\n",
               std::is_same_v<decltype(de_rvalue), rg::owning_view<std::vector<int>>>);

    fmt::print("      sizeof(ref_view)    = {} bytes (só um ponteiro)\n", sizeof(de_lvalue));
    fmt::print("      sizeof(owning_view) = {} bytes (o vector inteiro por dentro)\n",
               sizeof(de_rvalue));

    fmt::print("      Por isso copiar uma view é sempre barato quando ela\n");
    fmt::print("      referencia, e do tamanho do container quando ela possui.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Um pipeline de verdade
//
//     Processar um CSV sem alocar um vetor de strings intermediário.
// ─────────────────────────────────────────────────────────────────────────────
void demo_pipeline_real() {
    fmt::print("\n── 2.5 Um pipeline de verdade ──\n");

    const std::string csv =
        "ada,36,9500\n"
        "linus,28,8200\n"
        "#comentario a ignorar\n"
        "grace,45,11000\n"
        "\n"
        "barbara,52,12500\n";

    fmt::print("      entrada: {} bytes de CSV com comentários e linhas vazias\n",
               csv.size());

    // `std::to_address` em vez de `&*r.begin()`: a última linha do CSV produz
    // um subrange VAZIO cujo begin() == csv.end(), e desreferenciar o end() de
    // uma string é UB. `to_address` obtém o ponteiro SEM desreferenciar.
    auto para_sv = [](auto&& r) {
        return std::string_view(std::to_address(r.begin()),
                                static_cast<std::size_t>(rg::distance(r)));
    };

    auto registros = csv
        | vw::split('\n')
        | vw::transform(para_sv)
        | vw::filter([](std::string_view l) { return !l.empty() && l.front() != '#'; });

    fmt::print("      linhas úteis:\n");
    for (std::string_view linha : registros) {
        // Segundo nível de split, dentro do primeiro
        auto campos = linha | vw::split(',') | vw::transform(para_sv);
        auto it = campos.begin();
        std::string_view nome  = *it++;
        std::string_view idade = *it++;
        std::string_view sal   = *it;
        fmt::print("        {:<10} idade={:<3} salário={}\n", nome, idade, sal);
    }

    fmt::print("      Nenhuma std::string foi construída. Todos os campos são\n");
    fmt::print("      string_views apontando para dentro do buffer original.\n");
    fmt::print("      ⚠  o que exige que `csv` continue vivo — cuidado ao\n");
    fmt::print("         guardar esses views além do escopo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. O custo da abstração
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 2.6 O custo da abstração ──\n");

    fmt::print("      Em -O2, um pipeline de views compila para o MESMO código\n");
    fmt::print("      de um loop escrito à mão: cada adaptador é uma classe cujo\n");
    fmt::print("      operator++ é inlined, e o compilador funde tudo num só laço.\n");
    fmt::print("\n      Mas em -O0 (build de debug, que é o default deste repo)\n");
    fmt::print("      a diferença é grande: são várias camadas de chamada de\n");
    fmt::print("      função que ninguém inlina. Não meça views em debug.\n");
    fmt::print("\n      Onde views REALMENTE ganham, mesmo sem otimização:\n");
    fmt::print("        • eliminam containers temporários (menos alocação)\n");
    fmt::print("        • curto-circuitam com take/take_while (menos trabalho)\n");
    fmt::print("      Onde podem perder:\n");
    fmt::print("        • filter_view precisa achar o primeiro elemento válido\n");
    fmt::print("          já no begin() — o que faz `begin()` ser O(n)\n");
    fmt::print("        • pipelines muito longos estressam o compilador\n");
    fmt::print("          (tempo de compilação e mensagens de erro imensas)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_operator_pipe();
    demo_reutilizaveis();
    demo_dangling();
    demo_all();
    demo_pipeline_real();
    demo_custo();
}

} // namespace demo_pipelines
