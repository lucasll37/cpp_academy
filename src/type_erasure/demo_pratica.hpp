// =============================================================================
//  src/type_erasure/demo_pratica.hpp
//
//  Polimorfismo com semântica de VALOR
//  ───────────────────────────────────
//  A crítica de Sean Parent ("Inheritance Is The Base Class of Evil",
//  GoingNative 2013) em uma frase: herdar para ser polimórfico contamina o
//  seu tipo com ponteiro, heap e aliasing — problemas que ele não tinha.
//
//  A alternativa: o polimorfismo mora no CONTAINER, não no tipo guardado.
//  Você continua escrevendo structs comuns, copiáveis, sem vptr; a máquina
//  virtual fica escondida dentro de um tipo-valor.
//
//  Tópicos:
//  • Um `Desenhavel` que aceita QUALQUER coisa desenhável
//  • Como isto se compara com `unique_ptr<Base>`
//  • Concepts como contrato explícito
//  • ⚠ Quando NÃO fazer isto
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <concepts>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo_pratica {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Um tipo-valor polimórfico
//
//     Note que `Desenhavel` é COPIÁVEL, cabe num `vector` por valor e não
//     obriga ninguém a herdar de nada.
// ─────────────────────────────────────────────────────────────────────────────

// O contrato, explícito. Sem o concept, um tipo que não sirva produziria um
// erro dentro de `Modelo<T>` — dez níveis de template abaixo. Com ele, o erro
// é na construção e diz o que faltou. Ver src/concepts/.
template <typename T>
concept SabeDesenhar = requires(const T& t, int nivel) {
    { t.desenhar(nivel) } -> std::same_as<void>;
    { t.nome() }          -> std::convertible_to<std::string>;
};

class Desenhavel {
    struct Conceito {
        virtual ~Conceito() = default;
        virtual void                      desenhar(int) const = 0;
        virtual std::string               nome()        const = 0;
        virtual std::unique_ptr<Conceito> clonar()      const = 0;
    };

    template <typename T>
    struct Modelo final : Conceito {
        T v;
        explicit Modelo(T x) : v{std::move(x)} {}
        void        desenhar(int n) const override { v.desenhar(n); }
        std::string nome()          const override { return v.nome(); }
        std::unique_ptr<Conceito> clonar() const override {
            return std::make_unique<Modelo>(v);
        }
    };

    std::unique_ptr<Conceito> p_;

public:
    template <SabeDesenhar T>
    Desenhavel(T x)                                            // NOLINT: implícito
        : p_{std::make_unique<Modelo<T>>(std::move(x))} {}

    // Rule of 5 — é isto que dá semântica de valor
    Desenhavel(const Desenhavel& o) : p_{o.p_->clonar()} {}
    Desenhavel& operator=(const Desenhavel& o) {
        if (this != &o) p_ = o.p_->clonar();
        return *this;
    }
    Desenhavel(Desenhavel&&) noexcept            = default;
    Desenhavel& operator=(Desenhavel&&) noexcept = default;
    ~Desenhavel()                                = default;

    void        desenhar(int n = 0) const { p_->desenhar(n); }
    std::string nome()              const { return p_->nome(); }
};

// ── Os tipos guardados: structs comuns. Nenhum herda de nada. ───────────────
struct Ponto {
    double x{}, y{};
    void        desenhar(int n) const { fmt::print("{:{}}• ponto ({}, {})\n", "", n, x, y); }
    std::string nome()          const { return "Ponto"; }
};

struct Texto {
    std::string s;
    void        desenhar(int n) const { fmt::print("{:{}}\" {}\n", "", n, s); }
    std::string nome()          const { return "Texto"; }
};

// E este guarda OUTROS Desenhaveis — o Composite (src/design-of-pattern/composite/)
// construído sobre valores em vez de ponteiros.
struct Grupo {
    std::string            rotulo;
    std::vector<Desenhavel> filhos;

    void desenhar(int n) const {
        fmt::print("{:{}}[{}]\n", "", n, rotulo);
        for (const auto& f : filhos) f.desenhar(n + 2);
    }
    std::string nome() const { return "Grupo"; }
};

void demo_valor() {
    fmt::print("\n── 5.1 Polimorfismo com semântica de valor ──\n");

    std::vector<Desenhavel> cena;          // ← por VALOR, não unique_ptr
    cena.emplace_back(Ponto{1, 2});
    cena.emplace_back(Texto{"olá"});
    cena.emplace_back(Grupo{"sub", {Ponto{9, 9}, Texto{"aninhado"}}});

    fmt::print("      a cena:\n");
    for (const auto& d : cena) d.desenhar(6);

    // COPIAR a cena inteira. Com unique_ptr<Base> isto nem compilaria.
    fmt::print("\n      copiando a cena e alterando a cópia:\n");
    std::vector<Desenhavel> copia = cena;
    copia[0] = Texto{"substituído na cópia"};

    fmt::print("      original[0]: {}   |   cópia[0]: {}\n",
               cena[0].nome(), copia[0].nome());
    fmt::print("      A cópia foi PROFUNDA — inclusive do Grupo aninhado.\n");

    fmt::print("\n      `Ponto`, `Texto` e `Grupo` são structs comuns:\n");
    fmt::print("        sizeof(Ponto) = {} B, SEM vptr, copiável, comparável se\n",
               sizeof(Ponto));
    fmt::print("        você quiser, usável fora de qualquer cena.\n");
    fmt::print("      O polimorfismo mora no CONTAINER (`Desenhavel`), não neles.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O contraste com unique_ptr<Base>
// ─────────────────────────────────────────────────────────────────────────────
void demo_contraste() {
    fmt::print("\n── 5.2 O contraste com unique_ptr<Base> ──\n");

    fmt::print("      | aspecto                | unique_ptr<Base> | Desenhavel |\n");
    fmt::print("      |------------------------|------------------|------------|\n");
    fmt::print("      | copiável               | NÃO              | sim        |\n");
    fmt::print("      | o tipo precisa herdar  | sim              | não        |\n");
    fmt::print("      | funciona com int/string| não              | se atender |\n");
    fmt::print("      |                        |                  | ao concept |\n");
    fmt::print("      | pode ser nullptr       | sim              | não        |\n");
    fmt::print("      | == e < viáveis         | compara PONTEIRO | pode ser   |\n");
    fmt::print("      |                        |                  | por valor  |\n");
    fmt::print("      | aloca                  | sim              | sim        |\n\n");

    fmt::print("      \"pode ser nullptr\" merece atenção: `Desenhavel` NÃO tem\n");
    fmt::print("      construtor padrão, então não existe um \"desenhável vazio\".\n");
    fmt::print("      Isso elimina uma classe inteira de bugs — o mesmo argumento\n");
    fmt::print("      de `optional<T>` vs `T*` em src/vocabulary_types/.\n\n");

    fmt::print("      O que NÃO muda: os dois alocam no heap e pagam uma indireção.\n");
    fmt::print("      A medição de 4.1 mostrou os dois na mesma faixa (≈4× o CRTP).\n");
    fmt::print("      O ganho aqui é de PROJETO, não de velocidade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O concept como contrato
// ─────────────────────────────────────────────────────────────────────────────
void demo_concept() {
    fmt::print("\n── 5.3 O concept como contrato ──\n");

    fmt::print("      Sem o `requires`, um tipo inadequado produziria isto:\n\n");
    fmt::print("        error: 'struct Errado' has no member named 'desenhar'\n");
    fmt::print("        in instantiation of 'void Desenhavel::Modelo<T>::desenhar(int)\n");
    fmt::print("          const [with T = Errado]'\n");
    fmt::print("        required from 'Desenhavel::Modelo<Errado>'\n");
    fmt::print("        required from 'std::_MakeUniq<...>::__single_object\n");
    fmt::print("          std::make_unique<...>(...)'\n");
    fmt::print("        ... mais 8 linhas de contexto de template\n\n");

    fmt::print("      Com o concept `SabeDesenhar`:\n\n");
    fmt::print("        error: no matching function for call to\n");
    fmt::print("               'Desenhavel::Desenhavel(Errado)'\n");
    fmt::print("        note: constraints not satisfied\n");
    fmt::print("        note: the required expression 't.desenhar(nivel)' is invalid\n\n");

    fmt::print("      O erro voltou para a LINHA DO USUÁRIO e diz qual expressão\n");
    fmt::print("      faltou. É a diferença entre SFINAE e concepts descrita em\n");
    fmt::print("      src/concepts/demo_constraints.hpp.\n\n");

    fmt::print("      Bônus: o concept vira DOCUMENTAÇÃO executável. \"O que preciso\n");
    fmt::print("      implementar para caber aqui?\" tem resposta em 4 linhas de\n");
    fmt::print("      código, não num comentário que envelhece.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ Quando NÃO fazer isto
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_nao() {
    fmt::print("\n── 5.4 ⚠ Quando NÃO usar type erasure ──\n");

    fmt::print("      (a) Quando você CONTROLA todos os tipos e a lista é curta.\n");
    fmt::print("          `std::variant` é mais rápido (medido: 4× em 4.1), não\n");
    fmt::print("          aloca, e o `visit` reclama em compilação se você esquecer\n");
    fmt::print("          uma alternativa. Type erasure não tem exaustividade.\n\n");

    fmt::print("      (b) Quando o conjunto de operações é grande ou instável.\n");
    fmt::print("          Cada operação nova entra no `Conceito`, no `Modelo` e no\n");
    fmt::print("          wrapper: três lugares. Uma interface com 20 métodos vira\n");
    fmt::print("          60 linhas de repasse mecânico.\n\n");

    fmt::print("      (c) Quando não há heterogeneidade em runtime.\n");
    fmt::print("          Se o tipo é conhecido na chamada, um template resolve\n");
    fmt::print("          tudo em compilação e some. Não pague vtable à toa.\n\n");

    fmt::print("      (d) Quando \"virtual\" já resolve e é mais legível.\n");
    fmt::print("          Type erasure é ~40 linhas de boilerplate para o que\n");
    fmt::print("          `virtual` faz em 3. Só vale quando a NÃO-INTRUSIVIDADE\n");
    fmt::print("          ou a semântica de VALOR são de fato necessárias.\n\n");

    fmt::print("      Bibliotecas que geram o boilerplate por você:\n");
    fmt::print("        • `dyno`      (Louis Dionne) — vtable configurável\n");
    fmt::print("        • `Boost.TypeErasure` — composição de concepts\n");
    fmt::print("        • `folly::Poly`\n");
    fmt::print("      Sem reflexão na linguagem, todas custam macro ou muita\n");
    fmt::print("      metaprogramação. O C++26 deve mudar esse quadro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_valor();
    demo_contraste();
    demo_concept();
    demo_quando_nao();
}

}  // namespace demo_pratica
