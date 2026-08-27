// =============================================================================
//  src/vocabulary_types/demo_variant.hpp
//
//  std::variant<Ts...> — união com TIPO seguro
//  ───────────────────────────────────────────
//  Uma `union` de C guarda um de vários tipos no mesmo espaço, mas não guarda
//  QUAL. Ler o membro errado é undefined behavior, e nada impede.
//
//  `variant` guarda o mesmo espaço MAIS o índice do tipo ativo, e todo acesso
//  é verificado. É a soma de tipos ("um destes") que faltava no C++, do mesmo
//  jeito que `struct` é o produto ("todos estes").
//
//  E o mais importante: com `std::visit`, o compilador OBRIGA você a tratar
//  todos os casos. É polimorfismo com hierarquia FECHADA, sem herança, sem
//  vtable e sem alocação.
//
//  Tópicos:
//  • variant vs union — o que muda
//  • std::visit e a verificação de exaustividade
//  • O overload idiom — o truque que torna visit legível
//  • std::get, get_if, index, holds_alternative
//  • valueless_by_exception — o único estado inválido possível
//  • variant recursivo (uma árvore de expressão sem herança)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <stdexcept>
#include <utility>
#include <string>
#include <variant>
#include <vector>

namespace demo_variant {

// ─────────────────────────────────────────────────────────────────────────────
//  1. union vs variant
// ─────────────────────────────────────────────────────────────────────────────
union UnionCrua {
    int    i;
    double d;
    char   c;
};

void demo_vs_union() {
    fmt::print("\n── 2.1 union vs variant ──\n");

    UnionCrua u;
    u.i = 42;
    fmt::print("      union: escrevi u.i = 42\n");
    fmt::print("        ler u.i → {} ✓\n", u.i);
    fmt::print("        ler u.d → seria UNDEFINED BEHAVIOR (membro inativo)\n");
    fmt::print("        e NADA no código impede — a union não sabe quem está ativo\n");

    std::variant<int, double, char> v = 42;
    fmt::print("\n      variant: v = 42\n");
    fmt::print("        v.index()                 = {}  (0 = int)\n", v.index());
    fmt::print("        holds_alternative<int>    = {}\n", std::holds_alternative<int>(v));
    fmt::print("        holds_alternative<double> = {}\n", std::holds_alternative<double>(v));

    try {
        (void)std::get<double>(v);           // tipo errado → exceção, não UB
    } catch (const std::bad_variant_access& e) {
        fmt::print("        std::get<double> → bad_variant_access: {}\n", e.what());
    }

    fmt::print("\n      sizeof(UnionCrua)                    = {}\n", sizeof(UnionCrua));
    fmt::print("      sizeof(variant<int,double,char>)     = {}  (maior membro + índice)\n",
               sizeof(std::variant<int, double, char>));
    fmt::print("      O variant paga um índice e devolve segurança de tipo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Acessando o valor — quatro formas
// ─────────────────────────────────────────────────────────────────────────────
void demo_acesso() {
    fmt::print("\n── 2.2 As quatro formas de acessar ──\n");

    std::variant<int, std::string> v = std::string("texto");

    // (a) get_if — devolve ponteiro; nullptr se não for o tipo ativo. Não lança.
    if (const auto* s = std::get_if<std::string>(&v)) {
        fmt::print("      get_if<string>  → '{}'\n", *s);
    }
    if (std::get_if<int>(&v) == nullptr) {
        fmt::print("      get_if<int>     → nullptr (não é o ativo)\n");
    }

    // (b) get<T> — lança bad_variant_access
    fmt::print("      get<string>     → '{}'\n", std::get<std::string>(v));

    // (c) get<I> por índice — mesma semântica, útil quando há tipos repetidos
    fmt::print("      get<1>          → '{}'\n", std::get<1>(v));

    // (d) visit — o único que garante que TODOS os casos foram tratados
    std::visit([](const auto& x) {
        fmt::print("      visit           → '{}'\n", x);
    }, v);

    fmt::print("\n      | forma      | se tipo errado    | exaustividade |\n");
    fmt::print("      |------------|-------------------|---------------|\n");
    fmt::print("      | get_if<T>  | nullptr           | não checa     |\n");
    fmt::print("      | get<T>     | bad_variant_access| não checa     |\n");
    fmt::print("      | get<I>     | bad_variant_access| não checa     |\n");
    fmt::print("      | visit      | não acontece      | GARANTIDA     |\n");
    fmt::print("      `get<I>` acessa por ÍNDICE: é a única saída quando o\n");
    fmt::print("      mesmo tipo aparece duas vezes (ex.: variant<int,int>),\n");
    fmt::print("      caso em que `get<T>` nem compila.\n");
    fmt::print("      Prefira `visit`: é o único em que esquecer um caso é\n");
    fmt::print("      erro de COMPILAÇÃO.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O overload idiom — o truque que torna visit legível
//
//     `visit` exige um invocável que aceite TODOS os tipos. Com um lambda
//     genérico `[](const auto&)` isso é fácil, mas você perde o tratamento
//     por tipo. Escrever um struct com vários `operator()` funciona, mas é
//     verboso.
//
//     O overload idiom junta vários lambdas num só objeto, herdando o
//     `operator()` de cada um. São DUAS linhas de biblioteca.
// ─────────────────────────────────────────────────────────────────────────────

// Herda de todos os lambdas e traz os operator() de cada um para o escopo.
template <typename... Ts>
struct Overload : Ts... { using Ts::operator()...; };

// Deduction guide — em C++20 é opcional, mas explícito não custa.
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

using Valor = std::variant<int, double, std::string, bool>;

std::string descrever(const Valor& v) {
    return std::visit(Overload{
        [](int i)                { return fmt::format("inteiro {}", i); },
        [](double d)             { return fmt::format("real {:.2f}", d); },
        [](const std::string& s) { return fmt::format("texto '{}'", s); },
        [](bool b)               { return fmt::format("booleano {}", b); },
    }, v);
}

void demo_overload_idiom() {
    fmt::print("\n── 2.3 O overload idiom ──\n");

    const std::vector<Valor> valores{42, 3.14159, std::string("olá"), true};

    for (const auto& v : valores) {
        fmt::print("      {}\n", descrever(v));
    }

    fmt::print("\n      A definição inteira do idiom:\n");
    fmt::print("        template <typename... Ts>\n");
    fmt::print("        struct Overload : Ts... {{ using Ts::operator()...; }};\n");
    fmt::print("      Duas linhas. Herda de cada lambda e expõe o operator() de\n");
    fmt::print("      todos — a resolução de overload normal faz o resto.\n");

    // Um caso a mais no variant sem um caso a mais no visit = erro de compilação.
    fmt::print("\n      ⚠  se você acrescentar um tipo ao `Valor` e esquecer o\n");
    fmt::print("         lambda correspondente, `descrever` NÃO COMPILA. É a\n");
    fmt::print("         verificação de exaustividade que o `switch` não dá.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. visit com múltiplos variants — despacho duplo de graça
//
//     Este é o padrão Visitor do GoF, sem hierarquia, sem vtable, e sem os
//     dois níveis de indireção. O compilador gera a matriz de combinações.
// ─────────────────────────────────────────────────────────────────────────────
struct Pedra {}; struct Papel {}; struct Tesoura {};
using Jogada = std::variant<Pedra, Papel, Tesoura>;

std::string disputar(const Jogada& a, const Jogada& b) {
    return std::visit(Overload{
        [](Pedra,   Tesoura) { return "A vence (pedra quebra tesoura)"; },
        [](Papel,   Pedra)   { return "A vence (papel embrulha pedra)"; },
        [](Tesoura, Papel)   { return "A vence (tesoura corta papel)"; },
        [](Tesoura, Pedra)   { return "B vence (pedra quebra tesoura)"; },
        [](Pedra,   Papel)   { return "B vence (papel embrulha pedra)"; },
        [](Papel,   Tesoura) { return "B vence (tesoura corta papel)"; },
        [](auto,    auto)    { return "empate"; },   // catch-all para os iguais
    }, a, b);
}

void demo_visit_duplo() {
    fmt::print("\n── 2.4 visit com dois variants (despacho duplo) ──\n");

    const std::vector<std::pair<Jogada, Jogada>> partidas{
        {Pedra{},   Tesoura{}},
        {Papel{},   Tesoura{}},
        {Tesoura{}, Tesoura{}},
    };

    for (const auto& [a, b] : partidas) {
        fmt::print("      {}\n", disputar(a, b));
    }

    fmt::print("      O compilador gerou a matriz 3×3 de combinações.\n");
    fmt::print("      Com herança, isto seria o padrão Visitor: 2 hierarquias,\n");
    fmt::print("      um `accept()` por classe, e despacho duplo escrito à mão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. valueless_by_exception — o único estado inválido
//
//     Se a construção do novo valor lançar DURANTE uma atribuição, o antigo
//     já foi destruído e o novo não existe. O variant fica "sem valor".
//     É raro, mas é o motivo de `index()` poder devolver variant_npos.
// ─────────────────────────────────────────────────────────────────────────────
struct LancaNaCopia {
    LancaNaCopia() = default;
    LancaNaCopia(const LancaNaCopia&) { throw std::runtime_error("falhei na cópia"); }
    LancaNaCopia& operator=(const LancaNaCopia&) = default;
};

void demo_valueless() {
    fmt::print("\n── 2.5 valueless_by_exception ──\n");

    std::variant<int, LancaNaCopia> v = 42;
    fmt::print("      antes:  index={} valueless={}\n", v.index(), v.valueless_by_exception());

    try {
        const LancaNaCopia problema;
        v = problema;                    // a cópia lança no meio da atribuição
    } catch (const std::exception& e) {
        fmt::print("      exceção durante a atribuição: {}\n", e.what());
    }

    fmt::print("      depois: index={} valueless={}\n",
               v.index() == std::variant_npos ? -1 : static_cast<long>(v.index()),
               v.valueless_by_exception());

    if (v.valueless_by_exception()) {
        fmt::print("      ↑ variant_npos: nenhum tipo está ativo.\n");
        fmt::print("        `visit` sobre ele lança bad_variant_access.\n");
    }

    fmt::print("      Como evitar: mantenha o primeiro tipo do variant trivial\n");
    fmt::print("      (int, monostate) e prefira `emplace` a atribuição, ou use\n");
    fmt::print("      tipos com move `noexcept` — que é a Rule of 5 de novo\n");
    fmt::print("      (src/value_semantics/demo_rule_of_five.hpp).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. std::monostate — dando ao variant um estado "vazio" legítimo
//
//     `variant` não tem construtor default se o primeiro tipo não tiver.
//     `monostate` é um tipo vazio que serve de primeira alternativa.
// ─────────────────────────────────────────────────────────────────────────────
struct SemDefault { explicit SemDefault(int) {} };

void demo_monostate() {
    fmt::print("\n── 2.6 std::monostate ──\n");

    // std::variant<SemDefault, int> v;   // ✗ não compila: SemDefault não tem default ctor
    std::variant<std::monostate, SemDefault, int> v;   // ✓ default = monostate

    fmt::print("      variant<monostate, SemDefault, int> v;  → index={} (monostate)\n",
               v.index());

    v = 7;
    fmt::print("      após v = 7                             → index={}\n", v.index());

    v = std::monostate{};
    fmt::print("      voltando a \"vazio\"                     → index={}\n", v.index());

    fmt::print("      `monostate` é um struct vazio: sizeof = {}\n", sizeof(std::monostate));
    fmt::print("      Use quando o variant precisa de um estado \"nada ainda\".\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Variant recursivo — uma AST sem herança
//
//     Um variant não pode conter a si mesmo diretamente (o tamanho seria
//     infinito). A saída é a indireção: guardar um `unique_ptr` para o nó.
//
//     Compare com src/design-of-pattern/interpreter/, que faz a mesma árvore
//     com hierarquia virtual.
// ─────────────────────────────────────────────────────────────────────────────
struct Soma;
struct Multiplicacao;

using Expressao = std::variant<double,
                               std::unique_ptr<Soma>,
                               std::unique_ptr<Multiplicacao>>;

struct Soma           { Expressao esq, dir; };
struct Multiplicacao  { Expressao esq, dir; };

double avaliar(const Expressao& e) {
    return std::visit(Overload{
        [](double d) { return d; },
        [](const std::unique_ptr<Soma>& s) {
            return avaliar(s->esq) + avaliar(s->dir);
        },
        [](const std::unique_ptr<Multiplicacao>& m) {
            return avaliar(m->esq) * avaliar(m->dir);
        },
    }, e);
}

std::string imprimir(const Expressao& e) {
    return std::visit(Overload{
        [](double d) { return fmt::format("{:g}", d); },
        [](const std::unique_ptr<Soma>& s) {
            return fmt::format("({} + {})", imprimir(s->esq), imprimir(s->dir));
        },
        [](const std::unique_ptr<Multiplicacao>& m) {
            return fmt::format("({} * {})", imprimir(m->esq), imprimir(m->dir));
        },
    }, e);
}

void demo_recursivo() {
    fmt::print("\n── 2.7 Variant recursivo — uma AST sem herança ──\n");

    // (2 + 3) * 4
    Expressao ast = std::make_unique<Multiplicacao>(Multiplicacao{
        std::make_unique<Soma>(Soma{2.0, 3.0}),
        4.0
    });

    fmt::print("      expressão: {}\n", imprimir(ast));
    fmt::print("      valor:     {:g}\n", avaliar(ast));

    fmt::print("\n      Nenhuma classe base, nenhum método virtual, nenhum\n");
    fmt::print("      `accept()`. A hierarquia é FECHADA — e é justamente isso\n");
    fmt::print("      que permite ao compilador checar a exaustividade.\n");

    fmt::print("\n      | critério              | variant   | herança virtual |\n");
    fmt::print("      |-----------------------|-----------|-----------------|\n");
    fmt::print("      | tipos novos externos  | não       | SIM             |\n");
    fmt::print("      | operações novas       | SIM       | recompila tudo  |\n");
    fmt::print("      | exaustividade checada | SIM       | não             |\n");
    fmt::print("      | alocação por nó       | opcional  | sempre          |\n");
    fmt::print("      É o \"expression problem\": variant favorece OPERAÇÕES\n");
    fmt::print("      novas, herança favorece TIPOS novos. Escolha pelo eixo\n");
    fmt::print("      que vai variar mais.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_vs_union();
    demo_acesso();
    demo_overload_idiom();
    demo_visit_duplo();
    demo_valueless();
    demo_monostate();
    demo_recursivo();
}

} // namespace demo_variant
