// =============================================================================
//  src/vocabulary_types/demo_tuple.hpp
//
//  std::tuple e std::pair — agregados anônimos
//  ───────────────────────────────────────────
//  Um `struct` nomeia os campos; uma `tuple` não. Essa é a única diferença
//  relevante — e ela decide quando usar cada um.
//
//  A tuple existe para os casos em que dar nome seria burocracia: devolver
//  dois valores de uma função interna, empacotar argumentos para repassar,
//  gerar uma comparação lexicográfica. Onde o agregado é parte do DOMÍNIO,
//  um struct nomeado quase sempre ganha em legibilidade.
//
//  Tópicos:
//  • pair, tuple e o acesso por índice
//  • Structured bindings — o que tornou a tuple utilizável
//  • std::tie: desempate lexicográfico e ignorar campos
//  • std::apply e make_from_tuple
//  • Percorrer uma tuple (fold expressions e index_sequence)
//  • tuple vs struct — a tabela de decisão
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace demo_tuple {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O básico — e por que o acesso por índice envelheceu mal
// ─────────────────────────────────────────────────────────────────────────────
std::tuple<std::string, int, double> obter_registro() {
    return {"Ada", 36, 9500.0};
}

void demo_basico() {
    fmt::print("\n── 4.1 pair, tuple e acesso ──\n");

    std::pair<std::string, int> p{"chave", 42};
    fmt::print("      pair:  first='{}' second={}\n", p.first, p.second);

    auto t = obter_registro();

    // Acesso por índice — funciona, e é ilegível
    fmt::print("      tuple por índice: get<0>='{}' get<1>={} get<2>={}\n",
               std::get<0>(t), std::get<1>(t), std::get<2>(t));

    // Acesso por TIPO — só funciona se o tipo for único na tuple
    fmt::print("      tuple por tipo:   get<int>={}\n", std::get<int>(t));

    fmt::print("      tuple_size = {}\n", std::tuple_size_v<decltype(t)>);

    fmt::print("      ⚠  `get<1>` não diz NADA sobre o que é o campo 1.\n");
    fmt::print("         Antes do C++17 este era o custo real da tuple, e o\n");
    fmt::print("         motivo de todo mundo preferir struct. Structured\n");
    fmt::print("         bindings mudaram essa conta — próxima seção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Structured bindings (C++17) — o que salvou a tuple
// ─────────────────────────────────────────────────────────────────────────────
void demo_structured_bindings() {
    fmt::print("\n── 4.2 Structured bindings ──\n");

    // Agora os campos têm nome no ponto de USO, que é onde importa.
    auto [nome, idade, salario] = obter_registro();
    fmt::print("      auto [nome, idade, salario] = obter_registro();\n");
    fmt::print("        → '{}', {}, {:.2f}\n", nome, idade, salario);

    // Funciona com pair, tuple, array E struct — inclusive struct sem métodos
    struct Ponto { int x, y; };
    const Ponto pt{3, 4};
    auto [x, y] = pt;
    fmt::print("      também funciona em struct: x={} y={}\n", x, y);

    // O uso mais comum no dia a dia: iterar um map
    const std::map<std::string, int> estoque{{"maçã", 5}, {"pêra", 3}};
    fmt::print("      iterando um map:\n");
    for (const auto& [fruta, qtd] : estoque) {
        fmt::print("        {:<6} → {}\n", fruta, qtd);
    }

    // E o retorno de insert / try_emplace, que devolve pair<iterator,bool>
    std::map<std::string, int> m;
    if (auto [it, inserido] = m.try_emplace("nova", 1); inserido) {
        fmt::print("      try_emplace: inseriu '{}' = {}\n", it->first, it->second);
    }

    fmt::print("\n      ⚠  os nomes são COMPLETAMENTE novos, não aliases dos\n");
    fmt::print("         campos originais. `auto [a,b]` COPIA; `auto& [a,b]`\n");
    fmt::print("         referencia; `const auto& [a,b]` é o default seguro.\n");

    // Prova: por valor copia, por referência não
    auto reg = obter_registro();
    auto        [n1, i1, s1] = reg;   // cópia
    auto&       [n2, i2, s2] = reg;   // referência
    i1 = 99;                          // muda só a cópia
    i2 = 77;                          // muda a tuple original
    (void)n1; (void)s1; (void)n2; (void)s2;
    fmt::print("         após i1=99 (cópia) e i2=77 (ref): tuple tem {}\n",
               std::get<1>(reg));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. std::tie — desempate lexicográfico e descarte
// ─────────────────────────────────────────────────────────────────────────────
struct Pessoa {
    std::string nome;
    int         idade;
    double      salario;
};

void demo_tie() {
    fmt::print("\n── 4.3 std::tie ──\n");

    std::vector<Pessoa> equipe{
        {"Ada", 36, 9500}, {"Linus", 28, 8200},
        {"Grace", 45, 11000}, {"Dennis", 28, 7800},
    };

    // Uso 1: comparação por múltiplos critérios, de graça.
    // `tie` cria uma tuple de REFERÊNCIAS (sem copiar), e a tuple já tem
    // operator< lexicográfico.
    std::sort(equipe.begin(), equipe.end(), [](const Pessoa& a, const Pessoa& b) {
        return std::tie(a.idade, a.nome) < std::tie(b.idade, b.nome);
    });
    fmt::print("      ordenado por (idade, nome): ");
    for (const auto& p : equipe) fmt::print("{}({}) ", p.nome, p.idade);
    fmt::print("\n");

    fmt::print("      A alternativa escrita à mão:\n");
    fmt::print("        if (a.idade != b.idade) return a.idade < b.idade;\n");
    fmt::print("        return a.nome < b.nome;\n");
    fmt::print("      Com 4 critérios, isso vira 7 linhas e um bug esperando.\n");

    // Uso 2: desempacotar em variáveis que JÁ existem (bindings não fazem isso)
    std::string nome; int idade; double salario;
    std::tie(nome, idade, salario) = obter_registro();
    fmt::print("      tie em variáveis existentes: '{}', {}, {:.0f}\n", nome, idade, salario);

    // Uso 3: std::ignore para descartar campos
    int so_a_idade;
    std::tie(std::ignore, so_a_idade, std::ignore) = obter_registro();
    fmt::print("      tie com std::ignore: peguei só a idade = {}\n", so_a_idade);

    fmt::print("      ⚠  `tie` guarda REFERÊNCIAS: nunca devolva o resultado de\n");
    fmt::print("         `tie` de uma função — as referências ficam penduradas.\n");
    fmt::print("         Para devolver, use `make_tuple` (que copia).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::apply e make_from_tuple — a tuple como pacote de argumentos
// ─────────────────────────────────────────────────────────────────────────────
double calcular(int quantidade, double preco, double desconto) {
    return quantidade * preco * (1.0 - desconto);
}

struct Retangulo {
    double largura, altura;
    Retangulo(double l, double a) : largura(l), altura(a) {}
    double area() const { return largura * altura; }
};

void demo_apply() {
    fmt::print("\n── 4.4 std::apply e make_from_tuple ──\n");

    // apply: desempacota a tuple nos parâmetros da função
    const auto args = std::make_tuple(10, 25.0, 0.20);
    fmt::print("      apply(calcular, tuple(10, 25.0, 0.20)) = {:.2f}\n",
               std::apply(calcular, args));

    // make_from_tuple: o mesmo, mas para um CONSTRUTOR
    const auto dims = std::make_tuple(3.0, 4.0);
    const auto r = std::make_from_tuple<Retangulo>(dims);
    fmt::print("      make_from_tuple<Retangulo>(tuple(3,4)).area() = {:.0f}\n", r.area());

    // apply com um lambda genérico serve para "fazer algo com todos os campos"
    fmt::print("      apply com lambda variádico:\n        ");
    std::apply([](const auto&... campos) {
        ((fmt::print("[{}] ", campos)), ...);      // fold expression sobre operator,
    }, obter_registro());
    fmt::print("\n");

    fmt::print("      É assim que se implementa um wrapper genérico que\n");
    fmt::print("      guarda argumentos e chama depois — a base de `std::bind`,\n");
    fmt::print("      de thread pools e de sistemas de tarefas.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Percorrer uma tuple
//
//     Uma tuple não é iterável: cada elemento tem um TIPO diferente, então não
//     existe um `operator++` que faça sentido. Percorrer exige desdobrar em
//     compilação — com fold expression (C++17) ou index_sequence.
// ─────────────────────────────────────────────────────────────────────────────
template <typename Tupla, typename F>
void para_cada(const Tupla& t, F f) {
    // std::apply desempacota; a fold expression aplica f a cada elemento
    std::apply([&f](const auto&... elementos) { (f(elementos), ...); }, t);
}

// Versão com índice, quando você precisa saber a POSIÇÃO
template <typename Tupla, typename F, std::size_t... I>
void para_cada_indexado_impl(const Tupla& t, F f, std::index_sequence<I...>) {
    (f(I, std::get<I>(t)), ...);
}

template <typename Tupla, typename F>
void para_cada_indexado(const Tupla& t, F f) {
    para_cada_indexado_impl(t, f, std::make_index_sequence<std::tuple_size_v<Tupla>>{});
}

void demo_percorrer() {
    fmt::print("\n── 4.5 Percorrer uma tuple ──\n");

    const auto t = std::make_tuple(42, 3.14, std::string("texto"), true);

    fmt::print("      para_cada:          ");
    para_cada(t, [](const auto& x) { fmt::print("{} ", x); });
    fmt::print("\n");

    fmt::print("      para_cada_indexado:\n");
    para_cada_indexado(t, [](std::size_t i, const auto& x) {
        fmt::print("        [{}] {}\n", i, x);
    });

    fmt::print("      Não existe `for (auto x : tuple)`: cada elemento tem um\n");
    fmt::print("      TIPO diferente, então o corpo do laço precisa ser um\n");
    fmt::print("      template instanciado por elemento. É o que a fold\n");
    fmt::print("      expression `(f(x), ...)` faz — desdobra em compilação.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. tuple ou struct?
// ─────────────────────────────────────────────────────────────────────────────
// A alternativa nomeada, para comparação direta
struct ResultadoDaBusca {
    bool        encontrou;
    std::size_t indice;
    double      pontuacao;
};

void demo_tuple_ou_struct() {
    fmt::print("\n── 4.6 tuple ou struct? ──\n");

    fmt::print("      Compare os dois retornos:\n\n");
    fmt::print("        std::tuple<bool, size_t, double> buscar();\n");
    fmt::print("        auto [ok, i, p] = buscar();      // ok no ponto de uso\n");
    fmt::print("        auto r = buscar();\n");
    fmt::print("        if (std::get<0>(r)) ...          // ✗ ilegível\n\n");
    fmt::print("        ResultadoDaBusca buscar();\n");
    fmt::print("        auto r = buscar();\n");
    fmt::print("        if (r.encontrou) ...             // ✓ auto-documentado\n");

    fmt::print("\n      | use tuple/pair quando…        | use struct quando…       |\n");
    fmt::print("      |-------------------------------|--------------------------|\n");
    fmt::print("      | os campos não têm nome natural| o agregado é do DOMÍNIO  |\n");
    fmt::print("      | é interno e efêmero           | atravessa uma API pública|\n");
    fmt::print("      | precisa de tie/apply          | tem invariantes a manter |\n");
    fmt::print("      | é código genérico sobre N tipos| tem métodos próprios    |\n");
    fmt::print("      | 2 valores óbvios (pair)       | 3+ campos                |\n");

    fmt::print("\n      Desde o C++20, a inicialização designada tirou o último\n");
    fmt::print("      argumento a favor da tuple em código de aplicação:\n");
    fmt::print("        return ResultadoDaBusca{{.encontrou = true, .indice = 3,\n");
    fmt::print("                                .pontuacao = 0.91}};\n");

    // Prova de que o struct não custa nada a mais
    static_assert(sizeof(ResultadoDaBusca) >= sizeof(bool) + sizeof(std::size_t));
    fmt::print("\n      sizeof(tuple<bool,size_t,double>) = {}\n",
               sizeof(std::tuple<bool, std::size_t, double>));
    fmt::print("      sizeof(ResultadoDaBusca)          = {}  ← mesmo custo\n",
               sizeof(ResultadoDaBusca));
    fmt::print("      A escolha é 100% de legibilidade, não de performance.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_structured_bindings();
    demo_tie();
    demo_apply();
    demo_percorrer();
    demo_tuple_ou_struct();
}

} // namespace demo_tuple
