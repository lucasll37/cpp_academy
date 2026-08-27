// =============================================================================
//  src/lambdas/demo_invoke.hpp
//
//  std::invoke, bind_front e ponteiros para membro
//  ───────────────────────────────────────────────
//  "Chamável" em C++ é mais amplo do que parece. Além de funções, lambdas e
//  functors, também são chamáveis os PONTEIROS PARA MEMBRO — e eles têm uma
//  sintaxe de chamada completamente diferente:
//
//      f(a, b)           // função
//      (obj.*pm)(b)      // ponteiro para método
//      (obj.*pd)         // ponteiro para campo
//
//  Escrever código genérico que aceite todos exigiria três caminhos. O
//  `std::invoke` unifica: ele é a definição de "chamar" que a biblioteca
//  padrão usa internamente em toda parte.
//
//  Tópicos:
//  • Ponteiros para membro — sintaxe e utilidade
//  • std::invoke: a chamada universal
//  • std::invoke_result e is_invocable
//  • bind_front (C++20) vs bind (C++11) vs lambda
//  • Por que std::bind saiu de moda
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_invoke {

struct Pessoa {
    std::string nome;
    int         idade;

    std::string saudacao(const std::string& prefixo) const {
        return prefixo + ", " + nome;
    }
    void envelhecer(int anos) { idade += anos; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Ponteiros para membro
// ─────────────────────────────────────────────────────────────────────────────
void demo_ponteiros_membro() {
    fmt::print("\n── 4.1 Ponteiros para membro ──\n");

    Pessoa p{"Ada", 36};

    // Ponteiro para CAMPO: o tipo inclui a classe
    int Pessoa::* p_idade = &Pessoa::idade;
    std::string Pessoa::* p_nome = &Pessoa::nome;

    // Ponteiro para MÉTODO: tipo inclui classe, retorno, parâmetros e const
    std::string (Pessoa::* p_saudacao)(const std::string&) const = &Pessoa::saudacao;

    fmt::print("      objeto: nome='{}' idade={}\n", p.nome, p.idade);
    fmt::print("      via ponteiro para campo:  p.*p_idade = {}\n", p.*p_idade);
    fmt::print("      via ponteiro para campo:  p.*p_nome  = '{}'\n", p.*p_nome);
    fmt::print("      via ponteiro para método: (p.*p_saudacao)(\"Olá\") = '{}'\n",
               (p.*p_saudacao)("Olá"));

    // Com um PONTEIRO para o objeto, o operador é ->*
    Pessoa* ptr = &p;
    fmt::print("      com ponteiro p/ objeto:   ptr->*p_idade = {}\n", ptr->*p_idade);

    fmt::print("\n      A sintaxe é feia, mas o uso prático é ótimo — nos\n");
    fmt::print("      algoritmos de ranges, um ponteiro para membro é a\n");
    fmt::print("      PROJEÇÃO mais curta possível:\n");

    std::vector<Pessoa> equipe{{"Ada", 36}, {"Linus", 28}, {"Grace", 45}};
    std::ranges::sort(equipe, {}, &Pessoa::idade);        // ← ponteiro p/ membro
    fmt::print("        ranges::sort(equipe, {{}}, &Pessoa::idade): ");
    for (const auto& q : equipe) fmt::print("{}({}) ", q.nome, q.idade);
    fmt::print("\n");

    // E em <algorithm> clássico, via std::mem_fn ou invoke
    const auto total = std::accumulate(equipe.begin(), equipe.end(), 0,
        [](int acc, const Pessoa& q) { return acc + q.idade; });
    fmt::print("        soma das idades = {}\n", total);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::invoke — a chamada universal
//
//     Uma única sintaxe para os cinco casos que a linguagem trata diferente.
// ─────────────────────────────────────────────────────────────────────────────
int livre(int x) { return x * 2; }

void demo_invoke_universal() {
    fmt::print("\n── 4.2 std::invoke ──\n");

    Pessoa p{"Grace", 45};
    auto lambda = [](int x) { return x + 1; };
    struct Functor { int operator()(int x) const { return x * 10; } };

    fmt::print("      invoke(função livre, 21)          = {}\n", std::invoke(livre, 21));
    fmt::print("      invoke(lambda, 41)                = {}\n", std::invoke(lambda, 41));
    fmt::print("      invoke(functor, 4)                = {}\n", std::invoke(Functor{}, 4));
    fmt::print("      invoke(&Pessoa::idade, p)         = {}\n",
               std::invoke(&Pessoa::idade, p));
    fmt::print("      invoke(&Pessoa::saudacao, p, \"Oi\")= '{}'\n",
               std::invoke(&Pessoa::saudacao, p, "Oi"));

    fmt::print("\n      As cinco sintaxes que `invoke` unifica:\n");
    fmt::print("        f(args...)              função, lambda, functor\n");
    fmt::print("        (obj.*pm)(args...)      ponteiro p/ método\n");
    fmt::print("        (ptr->*pm)(args...)     idem, via ponteiro\n");
    fmt::print("        (obj.*pd)               ponteiro p/ campo\n");
    fmt::print("        (ptr->*pd)              idem, via ponteiro\n");
    fmt::print("      Sem `invoke`, código genérico precisaria de `if constexpr`\n");
    fmt::print("      para escolher entre elas.\n");

    // O caso que motiva: um wrapper genérico que funciona com QUALQUER chamável
    auto medir = []<typename F, typename... Args>(F&& f, Args&&... args) {
        return std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    };
    fmt::print("\n      um wrapper genérico usando invoke:\n");
    fmt::print("        medir(livre, 21)            = {}\n", medir(livre, 21));
    fmt::print("        medir(&Pessoa::idade, p)    = {}\n", medir(&Pessoa::idade, p));
    fmt::print("      O MESMO wrapper aceita função e ponteiro para membro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. is_invocable e invoke_result — perguntar antes de chamar
// ─────────────────────────────────────────────────────────────────────────────
void demo_traits() {
    fmt::print("\n── 4.3 is_invocable e invoke_result ──\n");

    auto f = [](int x, double y) { return x + y; };

    static_assert(std::is_invocable_v<decltype(f), int, double>);
    static_assert(!std::is_invocable_v<decltype(f), std::string>);
    static_assert(std::is_same_v<std::invoke_result_t<decltype(f), int, double>, double>);

    fmt::print("      is_invocable_v<F, int, double>      = {}\n",
               std::is_invocable_v<decltype(f), int, double>);
    fmt::print("      is_invocable_v<F, std::string>      = {}\n",
               std::is_invocable_v<decltype(f), std::string>);
    fmt::print("      invoke_result_t<F, int, double>     = double ✓\n");

    // is_invocable_r: checa também o tipo de RETORNO
    fmt::print("      is_invocable_r_v<int, F, int, double> = {}  ← double converte p/ int\n",
               std::is_invocable_r_v<int, decltype(f), int, double>);

    fmt::print("\n      Uso típico: restringir um template a chamáveis compatíveis.\n");
    fmt::print("        template <typename F>\n");
    fmt::print("          requires std::invocable<F, int>\n");
    fmt::print("        void processar(F f);\n");
    fmt::print("      Em C++20, o concept `std::invocable` faz isso direto.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. bind_front vs bind vs lambda
//
//     `std::bind` (C++11) resolvia a aplicação parcial antes de as lambdas
//     ficarem boas. Hoje ele perde para uma lambda em praticamente tudo, e o
//     C++20 trouxe `bind_front`/`bind_back` para o caso restante.
// ─────────────────────────────────────────────────────────────────────────────
int calcular(int base, int multiplicador, int offset) {
    return base * multiplicador + offset;
}

void demo_bind() {
    fmt::print("\n── 4.4 bind_front vs bind vs lambda ──\n");

    // (a) std::bind — placeholders, ordem livre
    using namespace std::placeholders;
    auto com_bind = std::bind(calcular, 10, _1, _2);
    fmt::print("      bind(calcular, 10, _1, _2)(3, 5)    = {}\n", com_bind(3, 5));

    // bind consegue REORDENAR os argumentos — a única coisa que só ele faz
    auto invertido = std::bind(calcular, _2, _1, 0);
    fmt::print("      bind(calcular, _2, _1, 0)(3, 10)    = {}  ← trocou a ordem\n",
               invertido(3, 10));

    // (b) bind_front (C++20) — fixa os PRIMEIROS argumentos, sem placeholders
    auto com_bind_front = std::bind_front(calcular, 10);
    fmt::print("      bind_front(calcular, 10)(3, 5)      = {}\n", com_bind_front(3, 5));

    // (c) lambda — a forma mais legível e a mais rápida
    auto com_lambda = [](int m, int o) { return calcular(10, m, o); };
    fmt::print("      [](int m,int o){{calcular(10,m,o);}}   = {}\n", com_lambda(3, 5));

    fmt::print("\n      | forma       | legível | inlinável | reordena | aloca  |\n");
    fmt::print("      |-------------|---------|-----------|----------|--------|\n");
    fmt::print("      | std::bind   | não     | mal       | SIM      | talvez |\n");
    fmt::print("      | bind_front  | ok      | sim       | não      | não    |\n");
    fmt::print("      | lambda      | SIM     | SIM       | sim*     | não    |\n");
    fmt::print("      * a lambda \"reordena\" simplesmente escrevendo na ordem certa.\n");

    fmt::print("\n      Por que `std::bind` saiu de moda:\n");
    fmt::print("        • os placeholders (`_1`, `_2`) são opacos na leitura\n");
    fmt::print("        • ele COPIA os argumentos ligados por padrão, e usar\n");
    fmt::print("          `std::ref` para evitar é fácil de esquecer\n");
    fmt::print("        • aninhar binds gera erros de template ilegíveis\n");
    fmt::print("        • ele trata `bind` aninhado de forma especial e\n");
    fmt::print("          surpreendente (composição implícita)\n");
    fmt::print("      A recomendação atual (Core Guidelines F.50): use lambda.\n");
    fmt::print("      `bind_front` só quando a aplicação parcial é o objetivo\n");
    fmt::print("      literal e você quer economizar a lista de parâmetros.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. mem_fn — o adaptador para métodos
// ─────────────────────────────────────────────────────────────────────────────
void demo_mem_fn() {
    fmt::print("\n── 4.5 std::mem_fn ──\n");

    std::vector<Pessoa> equipe{{"Ada", 36}, {"Linus", 28}, {"Grace", 45}};

    // mem_fn transforma um ponteiro para membro num functor comum
    auto obter_idade = std::mem_fn(&Pessoa::idade);
    fmt::print("      mem_fn(&Pessoa::idade)(equipe[0]) = {}\n", obter_idade(equipe[0]));

    // O que permite usá-lo onde se espera um functor de um argumento
    std::vector<int> idades(equipe.size());
    std::transform(equipe.begin(), equipe.end(), idades.begin(),
                   std::mem_fn(&Pessoa::idade));
    fmt::print("      transform com mem_fn → idades: ");
    for (int i : idades) fmt::print("{} ", i);
    fmt::print("\n");

    // Também funciona para métodos, e os argumentos extras são repassados
    auto envelhecer = std::mem_fn(&Pessoa::envelhecer);
    envelhecer(equipe[0], 10);
    fmt::print("      mem_fn(&Pessoa::envelhecer)(p, 10) → idade = {}\n",
               equipe[0].idade);

    fmt::print("\n      Hoje `mem_fn` é quase sempre substituível por uma lambda\n");
    fmt::print("      ou por uma PROJEÇÃO de ranges:\n");
    fmt::print("        std::transform(..., std::mem_fn(&Pessoa::idade));\n");
    fmt::print("        std::ranges::sort(v, {{}}, &Pessoa::idade);   ← mais direto\n");
    fmt::print("      Ver src/ranges/demo_algorithms.hpp.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_ponteiros_membro();
    demo_invoke_universal();
    demo_traits();
    demo_bind();
    demo_mem_fn();
}

} // namespace demo_invoke
