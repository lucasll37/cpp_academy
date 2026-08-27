// =============================================================================
//  src/operators/demo_comparacao.hpp
//
//  operator<=> — o spaceship do C++20
//  ──────────────────────────────────
//  Antes do C++20, dar ordem completa a um tipo exigia escrever SEIS operadores
//  (`==`, `!=`, `<`, `<=`, `>`, `>=`), quase sempre copiando a mesma lógica —
//  e bastava errar um sinal para o tipo ficar sutilmente inconsistente.
//
//  O `operator<=>` devolve uma CATEGORIA DE ORDENAÇÃO e o compilador deriva
//  todos os operadores relacionais dela. Com `= default`, ele até escreve a
//  comparação membro a membro.
//
//  Tópicos:
//  • As três categorias: strong, weak e partial_ordering
//  • `= default` e a comparação lexicográfica automática
//  • Por que `==` continua separado de `<=>`
//  • Candidatos reescritos e revertidos — como o compilador deriva
//  • Comparação heterogênea e a implementação manual
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <cmath>
#include <compare>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_comparacao {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O antes e o depois
// ─────────────────────────────────────────────────────────────────────────────
namespace antigo {

struct Versao {
    int maior, menor, patch;

    // Seis operadores. E cinco deles são só reescritas do primeiro.
    friend bool operator==(const Versao& a, const Versao& b) {
        return a.maior == b.maior && a.menor == b.menor && a.patch == b.patch;
    }
    friend bool operator!=(const Versao& a, const Versao& b) { return !(a == b); }
    friend bool operator<(const Versao& a, const Versao& b) {
        if (a.maior != b.maior) return a.maior < b.maior;
        if (a.menor != b.menor) return a.menor < b.menor;
        return a.patch < b.patch;
    }
    friend bool operator>(const Versao& a, const Versao& b)  { return b < a; }
    friend bool operator<=(const Versao& a, const Versao& b) { return !(b < a); }
    friend bool operator>=(const Versao& a, const Versao& b) { return !(a < b); }
};

} // namespace antigo

namespace moderno {

struct Versao {
    int maior, menor, patch;

    // UMA linha. O compilador compara os membros na ordem de DECLARAÇÃO e
    // gera <, <=, >, >=, == e != a partir daqui.
    auto operator<=>(const Versao&) const = default;
};

} // namespace moderno

void demo_antes_depois() {
    fmt::print("\n── 2.1 Seis operadores viram um ──\n");

    moderno::Versao a{1, 2, 3}, b{1, 3, 0};

    fmt::print("      Versao{{1,2,3}} vs Versao{{1,3,0}}:\n");
    fmt::print("        a <  b → {}\n", a < b);
    fmt::print("        a >  b → {}\n", a > b);
    fmt::print("        a == b → {}\n", a == b);
    fmt::print("        a != b → {}\n", a != b);
    fmt::print("        a <= b → {}\n", a <= b);

    fmt::print("      Tudo isso a partir de:\n");
    fmt::print("        auto operator<=>(const Versao&) const = default;\n");
    fmt::print("      A comparação é LEXICOGRÁFICA na ordem de DECLARAÇÃO dos\n");
    fmt::print("      membros — trocar a ordem dos campos muda a semântica.\n");

    // E ordenação funciona de graça
    std::vector<moderno::Versao> versoes{{2, 0, 0}, {1, 10, 0}, {1, 2, 3}, {1, 2, 10}};
    std::sort(versoes.begin(), versoes.end());
    fmt::print("      ordenadas: ");
    for (const auto& v : versoes) fmt::print("{}.{}.{} ", v.maior, v.menor, v.patch);
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. As três categorias de ordenação
//
//     A categoria é o TIPO devolvido por `<=>`, e ela documenta que garantias
//     a comparação oferece:
//
//     strong_ordering  — quaisquer dois valores são comparáveis, E `a == b`
//                        implica que a e b são indistinguíveis (substituíveis
//                        em qualquer contexto). É uma ORDEM TOTAL de verdade.
//     weak_ordering    — quaisquer dois valores são comparáveis, mas
//                        equivalentes podem ser distinguíveis (ex.: string
//                        ignorando maiúsculas). Formalmente é uma PREORDEM
//                        total: total sobre as classes de equivalência, não
//                        sobre os valores.
//     partial_ordering — pode haver pares INCOMPARÁVEIS (ex.: NaN)
// ─────────────────────────────────────────────────────────────────────────────

// strong: dois IDs iguais são o mesmo ID, ponto
struct Id {
    int valor;
    auto operator<=>(const Id&) const = default;      // strong_ordering
};

// weak: "ABC" e "abc" são equivalentes na ordenação, mas NÃO são iguais
struct NomeSemCaso {
    std::string texto;

    std::weak_ordering operator<=>(const NomeSemCaso& o) const {
        const auto minusculo = [](const std::string& s) {
            std::string r = s;
            for (char& c : r) if (c >= 'A' && c <= 'Z') c += 32;
            return r;
        };
        return minusculo(texto) <=> minusculo(o.texto);
    }
    bool operator==(const NomeSemCaso& o) const {
        return (*this <=> o) == 0;
    }
};

// partial: double já é partial_ordering por causa do NaN
struct Medida {
    double valor;
    auto operator<=>(const Medida&) const = default;   // partial_ordering
};

void demo_categorias() {
    fmt::print("\n── 2.2 As três categorias ──\n");

    static_assert(std::is_same_v<decltype(Id{1} <=> Id{2}), std::strong_ordering>);
    static_assert(std::is_same_v<decltype(Medida{1} <=> Medida{2}), std::partial_ordering>);

    fmt::print("      Id      → strong_ordering  (int é strong)\n");
    fmt::print("      Medida  → partial_ordering (double tem NaN)\n");
    fmt::print("      NomeSemCaso → weak_ordering (declarado à mão)\n");

    // weak: equivalentes na ordem, distinguíveis de fato
    NomeSemCaso x{"ABC"}, y{"abc"};
    fmt::print("\n      weak_ordering em ação:\n");
    fmt::print("        \"ABC\" <=> \"abc\" é equivalente? {}\n", (x <=> y) == 0);
    fmt::print("        mas as strings são idênticas?   {}\n", x.texto == y.texto);
    fmt::print("        ↑ equivalente ≠ igual. É essa a diferença strong/weak.\n");

    // partial: incomparável
    const double nan = std::nan("");
    Medida m1{1.0}, m2{nan};
    const auto r = m1 <=> m2;
    fmt::print("\n      partial_ordering em ação (com NaN):\n");
    fmt::print("        1.0 <  NaN → {}\n", r < 0);
    fmt::print("        1.0 >  NaN → {}\n", r > 0);
    fmt::print("        1.0 == NaN → {}\n", r == 0);
    fmt::print("        → TRÊS falsos. O resultado é `unordered`.\n");
    fmt::print("        is_eq/is_lt/is_gt todos falsos = incomparável.\n");

    fmt::print("\n      | categoria | todos comparáveis | equivalente = igual |\n");
    fmt::print("      |-----------|-------------------|---------------------|\n");
    fmt::print("      | strong    | sim               | SIM                 |\n");
    fmt::print("      | weak      | sim               | não                 |\n");
    fmt::print("      | partial   | NÃO               | não                 |\n");
    fmt::print("      Só `strong` é uma ORDEM TOTAL sobre os valores. `weak` é\n");
    fmt::print("      uma preordem total: ordena as CLASSES de equivalência.\n");
    fmt::print("      Quanto mais forte, mais o compilador e os containers\n");
    fmt::print("      podem otimizar. Prefira a mais forte que for verdadeira.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Por que `==` continua separado
//
//     Numa comparação lexicográfica, `<` precisa examinar os campos em ordem
//     até achar diferença. Mas `==` pode começar por um campo BARATO que
//     descarta rápido — por exemplo, o tamanho de uma string.
//
//     Por isso o padrão NÃO deriva `==` de `<=>` automaticamente quando você
//     escreve o `<=>` à mão: você provavelmente quer um `==` mais rápido.
//     (com `= default`, os dois são gerados.)
// ─────────────────────────────────────────────────────────────────────────────
class Texto {
public:
    explicit Texto(std::string s) : dados_(std::move(s)) {}

    // A ordenação precisa comparar caractere a caractere
    std::strong_ordering operator<=>(const Texto& o) const {
        ++comparacoes_ordem;
        return dados_ <=> o.dados_;
    }

    // Mas a IGUALDADE pode descartar pelo tamanho antes de olhar o conteúdo
    bool operator==(const Texto& o) const {
        ++comparacoes_igualdade;
        if (dados_.size() != o.dados_.size()) return false;   // atalho barato
        return dados_ == o.dados_;
    }

    static inline int comparacoes_ordem = 0;
    static inline int comparacoes_igualdade = 0;

private:
    std::string dados_;
};

void demo_igualdade_separada() {
    fmt::print("\n── 2.3 Por que `==` é separado de `<=>` ──\n");

    Texto a{"uma string bem longa para comparar"};
    Texto b{"curta"};

    fmt::print("      a == b → {}  (descartou pelo TAMANHO, sem olhar o conteúdo)\n",
               a == b);
    fmt::print("      a <  b → {}  (precisou comparar caractere a caractere)\n",
               a < b);

    fmt::print("\n      O padrão não deriva `==` do `<=>` escrito à mão porque\n");
    fmt::print("      a igualdade quase sempre tem um atalho mais barato:\n");
    fmt::print("        • string  → compare o tamanho primeiro\n");
    fmt::print("        • vector  → idem\n");
    fmt::print("        • árvore  → compare a contagem de nós\n");
    fmt::print("      Com `<=>() = default`, o compilador gera os DOIS.\n");
    fmt::print("      Se você escreve `<=>` à mão, precisa escrever `==` também.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Candidatos reescritos e revertidos
//
//     O mecanismo pelo qual UM operador vira seis. Ao ver `a < b`, o
//     compilador considera, nesta ordem:
//       1. `a < b`            — se existir um operator< próprio
//       2. `(a <=> b) < 0`    — candidato REESCRITO
//       3. `0 < (b <=> a)`    — candidato REVERTIDO (para tipos heterogêneos)
// ─────────────────────────────────────────────────────────────────────────────
struct Metros {
    double valor;

    // Comparação HOMOGÊNEA
    auto operator<=>(const Metros&) const = default;

    // Comparação HETEROGÊNEA: só declaramos Metros <=> double.
    // O compilador deriva `double < Metros` invertendo (candidato revertido).
    std::partial_ordering operator<=>(double d) const { return valor <=> d; }
    bool operator==(double d) const { return valor == d; }
};

void demo_reescrito_revertido() {
    fmt::print("\n── 2.4 Candidatos reescritos e revertidos ──\n");

    Metros m{5.0};

    fmt::print("      declaramos APENAS `Metros <=> double`. O compilador deriva:\n");
    fmt::print("        m <  7.0  → {}   (reescrito: (m <=> 7.0) < 0)\n",  m < 7.0);
    fmt::print("        m == 5.0  → {}   (do operator== declarado)\n",     m == 5.0);
    fmt::print("        7.0 >  m  → {}   (REVERTIDO: 0 < (m <=> 7.0))\n",  7.0 > m);
    fmt::print("        5.0 == m  → {}   (revertido do operator==)\n",     5.0 == m);

    fmt::print("\n      São 8 operações possíveis (4 relacionais × 2 ordens de\n");
    fmt::print("      operando) derivadas de DUAS declarações.\n");
    fmt::print("      Antes do C++20, isso exigiria 12 funções escritas à mão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Onde a ordenação é exigida: containers ordenados
// ─────────────────────────────────────────────────────────────────────────────
struct Prioridade {
    int          nivel;
    std::string  etiqueta;

    // Ordena por nível; desempata pela etiqueta. Uma linha.
    auto operator<=>(const Prioridade&) const = default;
};

void demo_containers() {
    fmt::print("\n── 2.5 Containers ordenados ──\n");

    // std::set exige apenas `<` — que o <=> derivou
    std::set<Prioridade> fila{
        {2, "compilar"}, {1, "urgente"}, {2, "avaliar"}, {3, "opcional"},
    };

    fmt::print("      std::set<Prioridade> ordenado automaticamente:\n");
    for (const auto& p : fila) fmt::print("        [{}] {}\n", p.nivel, p.etiqueta);

    fmt::print("\n      O `set` só precisa de `<`. O `<=> = default` deu isso,\n");
    fmt::print("      mais os outros cinco, sem nenhuma linha extra.\n");
    fmt::print("      ⚠  cuidado: a ordem dos MEMBROS na declaração define a\n");
    fmt::print("         ordem lexicográfica. Se `etiqueta` viesse antes de\n");
    fmt::print("         `nivel`, este set ordenaria alfabeticamente.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_antes_depois();
    demo_categorias();
    demo_igualdade_separada();
    demo_reescrito_revertido();
    demo_containers();
}

} // namespace demo_comparacao
