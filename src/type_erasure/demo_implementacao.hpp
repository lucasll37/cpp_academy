// =============================================================================
//  src/type_erasure/demo_implementacao.hpp
//
//  std::function por dentro — construindo do zero
//  ─────────────────────────────────────────────
//  A receita do apagamento de tipo tem sempre as mesmas três peças:
//
//    1. uma INTERFACE abstrata que descreve as operações (mas não o tipo);
//    2. um MODELO template<T> que implementa a interface delegando para T;
//    3. um WRAPPER com valor semântico que guarda um ponteiro para a interface.
//
//  O truque está na peça 2: `Modelo<T>` conhece T, herda de `Conceito` e some
//  atrás do ponteiro. Quem escreveu T nunca ouviu falar de nenhuma das três.
//
//  Tópicos:
//  • Conceito / Modelo / Wrapper
//  • Fazer o wrapper copiável: o `clone()`
//  • SBO — small buffer optimization
//  • A alternativa sem herança: vtable manual
//  • ⚠ move_only_function e por que ela existe
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdlib>
#include <fmt/format.h>
#include <memory>
#include <new>
#include <string>
#include <functional>
#include <type_traits>
#include <utility>

namespace demo_implementacao {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A receita completa
// ─────────────────────────────────────────────────────────────────────────────
namespace v1 {

template <typename Assinatura>
class Funcao;                                  // só declarada

template <typename R, typename... Args>
class Funcao<R(Args...)> {
    // ── PEÇA 1: o Conceito — o que sabemos fazer, sem saber com o quê ──────
    struct Conceito {
        virtual ~Conceito() = default;
        virtual R                          invocar(Args...) const = 0;
        virtual std::unique_ptr<Conceito>  clonar()          const = 0;
        virtual const char*                tipo()            const = 0;
    };

    // ── PEÇA 2: o Modelo — conhece T, implementa o Conceito ────────────────
    template <typename T>
    struct Modelo final : Conceito {
        T alvo;
        explicit Modelo(T t) : alvo{std::move(t)} {}

        R invocar(Args... a) const override {
            // const_cast porque um functor pode ter operator() não-const;
            // `std::function` faz o mesmo. `move_only_function` do C++23
            // corrigiu isso com qualificadores na assinatura.
            return const_cast<T&>(alvo)(std::forward<Args>(a)...);
        }
        std::unique_ptr<Conceito> clonar() const override {
            return std::make_unique<Modelo>(alvo);   // ← exige T copiável
        }
        const char* tipo() const override { return typeid(T).name(); }
    };

    std::unique_ptr<Conceito> pimpl_;

public:
    Funcao() = default;

    // O construtor template é o que aceita QUALQUER callable.
    // O `requires` evita que ele sequestre a cópia — sem ele,
    // `Funcao f2{f1}` escolheria este e criaria Funcao<Funcao<...>>.
    template <typename T>
        requires (!std::is_same_v<std::decay_t<T>, Funcao>)
    Funcao(T&& t)                                             // NOLINT: implícito de propósito
        : pimpl_{std::make_unique<Modelo<std::decay_t<T>>>(std::forward<T>(t))} {}

    // ── Rule of 5: cópia profunda via clonar() ─────────────────────────────
    Funcao(const Funcao& o) : pimpl_{o.pimpl_ ? o.pimpl_->clonar() : nullptr} {}
    Funcao& operator=(const Funcao& o) {
        if (this != &o) pimpl_ = o.pimpl_ ? o.pimpl_->clonar() : nullptr;
        return *this;
    }
    Funcao(Funcao&&) noexcept            = default;
    Funcao& operator=(Funcao&&) noexcept = default;
    ~Funcao()                            = default;

    R operator()(Args... a) const {
        if (!pimpl_) throw std::bad_function_call{};
        return pimpl_->invocar(std::forward<Args>(a)...);
    }

    explicit operator bool() const noexcept { return pimpl_ != nullptr; }
    const char* tipo_guardado() const { return pimpl_ ? pimpl_->tipo() : "vazio"; }
};

}  // namespace v1

void demo_receita() {
    fmt::print("\n── 2.1 Conceito / Modelo / Wrapper ──\n");

    struct Triplica { int operator()(int x) const { return x * 3; } };

    v1::Funcao<int(int)> f = Triplica{};
    fmt::print("      functor      : f(5) = {}\n", f(5));

    int base = 100;
    f = [base](int x) { return x + base; };
    fmt::print("      lambda       : f(5) = {}\n", f(5));

    f = +[](int x) { return -x; };
    fmt::print("      ponteiro     : f(5) = {}\n", f(5));

    // A cópia é PROFUNDA — cada uma tem o seu Modelo
    v1::Funcao<int(int)> g = f;
    f = Triplica{};
    fmt::print("      após copiar e reatribuir f: g(5) = {}, f(5) = {}\n", g(5), f(5));
    fmt::print("      (g manteve o seu próprio alvo: a cópia foi profunda)\n");

    v1::Funcao<int(int)> vazia;
    fmt::print("      vazia? bool = {}\n", static_cast<bool>(vazia));

    fmt::print("\n      As três peças:\n");
    fmt::print("        Conceito  → interface abstrata: invocar/clonar/tipo\n");
    fmt::print("        Modelo<T> → herda de Conceito, GUARDA um T, delega\n");
    fmt::print("        Funcao    → valor semântico, unique_ptr<Conceito>\n\n");
    fmt::print("      `Triplica` não herda de nada e não sabe que `Funcao` existe.\n");
    fmt::print("      O acoplamento é ZERO — só a exigência implícita de ser\n");
    fmt::print("      invocável com (int) e devolver algo conversível para int.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Por que `clonar()`
// ─────────────────────────────────────────────────────────────────────────────
void demo_clone() {
    fmt::print("\n── 2.2 O clone e o valor semântico ──\n");

    fmt::print("      `unique_ptr<Conceito>` não é copiável. Sem `clonar()`, a\n");
    fmt::print("      `Funcao` também não seria — e deixaria de ser um VALOR.\n\n");

    fmt::print("      Três saídas possíveis:\n");
    fmt::print("        (a) `clonar()` virtual   → cópia profunda, é o que fizemos;\n");
    fmt::print("                                   é o padrão Prototype\n");
    fmt::print("                                   (src/design-of-pattern/prototype/)\n");
    fmt::print("        (b) `shared_ptr<const Conceito>` → cópia rasa e barata,\n");
    fmt::print("                                   mas o alvo passa a ser compartilhado\n");
    fmt::print("        (c) proibir a cópia      → é o `std::move_only_function`\n");
    fmt::print("                                   do C++23\n\n");

    fmt::print("      `std::function` escolheu (a) — e paga o preço: ela EXIGE\n");
    fmt::print("      que o callable seja copiável. Isto NÃO compila:\n\n");
    fmt::print("        auto p = std::make_unique<int>(42);\n");
    fmt::print("        std::function<int()> f = [p = std::move(p)] {{ return *p; }};\n");
    fmt::print("        // erro: a lambda é move-only, function precisa copiar\n\n");
    fmt::print("      É exatamente esse buraco que o `std::move_only_function`\n");
    fmt::print("      do C++23 fecha — e o GCC 13 já o tem (ver src/cpp23/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. SBO — small buffer optimization
//
//     `std::function` não aloca para callables pequenos: ela tem um buffer
//     interno. Medimos quantos bytes cabem antes de o `new` aparecer.
// ─────────────────────────────────────────────────────────────────────────────
namespace sbo {

// Contador global de alocações. Substituímos `operator new`/`delete` globais —
// é a única forma de saber se a `std::function` alocou. O flag `ativo` mantém
// a contagem restrita ao trecho medido; sem ele, contaríamos também o que o
// fmt aloca ao imprimir.
//
// ⚠ A substituição do `operator new` global tem que ser ÚNICA no programa
//   inteiro (é uma função com linkage externa reservada). Por isso ela NÃO é
//   `inline` nem está num namespace: mora aqui porque este cabeçalho é
//   incluído uma única vez, por main.cpp. Ver src/linkage/ sobre a ODR.
inline bool        ativo      = false;
inline std::size_t alocacoes  = 0;
inline std::size_t bytes      = 0;

struct Escopo {
    Escopo()  { alocacoes = 0; bytes = 0; ativo = true;  }
    ~Escopo() { ativo = false; }
};

}  // namespace sbo

}  // namespace demo_implementacao — fechado de propósito, veja abaixo

// ⚠ A substituição do `operator new` global precisa estar no ESCOPO GLOBAL.
//   Declará-la dentro de um namespace é erro de compilação — ela não seria uma
//   substituição, seria outra função qualquer com o mesmo nome:
//     error: 'void* ns::operator new(size_t)' may not be declared within a namespace
//   E ela tem que ser ÚNICA no programa inteiro (linkage externa reservada);
//   por isso não é `inline`. Mora neste cabeçalho porque main.cpp o inclui uma
//   única vez. Ver src/linkage/ sobre a ODR.
// O GCC avisa `-Wmismatched-new-delete` ao inlinar estes corpos: ele vê um
// `free()` sobre um ponteiro que veio de `operator new` e não conclui que este
// `new` é justamente o que chamou `malloc`. É um falso positivo específico da
// substituição do par global — que é exatamente o que o padrão manda fazer.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"

void* operator new(std::size_t n) {
    if (demo_implementacao::sbo::ativo) {
        ++demo_implementacao::sbo::alocacoes;
        demo_implementacao::sbo::bytes += n;
    }
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc{};
}
// Os `delete` pareados também precisam ser substituídos: misturar o `new`
// substituído com o `delete` padrão é UB.
void operator delete(void* p) noexcept              { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

#pragma GCC diagnostic pop

namespace demo_implementacao {

void demo_sbo() {
    fmt::print("\n── 2.3 SBO — small buffer optimization ──\n");

    fmt::print("      sizeof(std::function<void()>)  = {} bytes\n",
               sizeof(std::function<void()>));
    fmt::print("      sizeof(v1::Funcao<void()>)     = {} bytes  (só o unique_ptr)\n",
               sizeof(v1::Funcao<void()>));

    fmt::print("\n      Quantas alocações a construção realmente faz? Medindo com\n");
    fmt::print("      um `operator new` global instrumentado:\n\n");

    // Cada caso constrói uma lambda com captura de tamanho conhecido e conta.
    auto medir = [](const char* rotulo, auto&& fabrica) {
        std::size_t n = 0, b = 0;
        {
            sbo::Escopo e;
            auto f = fabrica();         // ← a construção que estamos medindo
            n = sbo::alocacoes; b = sbo::bytes;
            (void)f;
        }
        fmt::print("        {:<26} → {} alocação(ões){}\n", rotulo, n,
                   n ? fmt::format(", {} bytes", b) : std::string{", INLINE no buffer"});
    };

    struct C8  { char d[8];  };
    struct C16 { char d[16]; };
    struct C24 { char d[24]; };
    struct C64 { char d[64]; };

    medir("std::function, sem captura", [] {
        return std::function<void()>{[] {}};
    });
    medir("std::function, captura 8B",  [] {
        C8 c{}; return std::function<void()>{[c] { (void)c; }};
    });
    medir("std::function, captura 16B", [] {
        C16 c{}; return std::function<void()>{[c] { (void)c; }};
    });
    medir("std::function, captura 24B", [] {
        C24 c{}; return std::function<void()>{[c] { (void)c; }};
    });
    medir("std::function, captura 64B", [] {
        C64 c{}; return std::function<void()>{[c] { (void)c; }};
    });
    medir("v1::Funcao, sem captura",    [] {
        return v1::Funcao<void()>{[] {}};
    });

    fmt::print("\n      O corte medido acima é o buffer interno da libstdc++.\n");
    fmt::print("      A `v1::Funcao` que escrevemos aloca SEMPRE — ela guarda um\n");
    fmt::print("      `unique_ptr<Conceito>` e nada mais. É o preço de usar\n");
    fmt::print("      `virtual`: o objeto guardado precisa de vptr, e vptr no\n");
    fmt::print("      buffer complicaria o move.\n\n");

    fmt::print("      A regra exata é da IMPLEMENTAÇÃO, não do padrão. A libstdc++\n");
    fmt::print("      ainda exige que o callable seja nothrow-move-constructible\n");
    fmt::print("      para usar o buffer: se o move puder lançar, ela aloca, porque\n");
    fmt::print("      mover a `function` teria que mover o buffer.\n");
    fmt::print("      (mesmo raciocínio de `vector` em src/value_semantics/)\n\n");

    fmt::print("      Consequência prática: uma lambda que captura `[this]` ou um\n");
    fmt::print("      `int` é gratuita dentro de `std::function`. Uma que captura\n");
    fmt::print("      três `std::string` por valor aloca.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Sem herança: vtable manual
//
//     A versão v1 usa `virtual`. Dá para apagar o tipo sem herança nenhuma:
//     basta montar a tabela de ponteiros de função à mão. É como a libstdc++
//     realmente faz, e é o que permite o SBO (sem vptr no objeto guardado).
// ─────────────────────────────────────────────────────────────────────────────
namespace v2 {

template <typename R, typename... Args>
class Funcao {
    // A "vtable": um struct de ponteiros de função, UMA instância estática
    // por tipo T. Sem `virtual`, sem vptr no objeto.
    struct VTable {
        R    (*invocar)(void*, Args...);
        void (*destruir)(void*);
        void*(*clonar)(void*);
    };

    template <typename T>
    static const VTable* tabela_de() {
        static constexpr VTable t{
            +[](void* p, Args... a) -> R {
                return (*static_cast<T*>(p))(std::forward<Args>(a)...);
            },
            +[](void* p) { delete static_cast<T*>(p); },
            +[](void* p) -> void* { return new T{*static_cast<T*>(p)}; },
        };
        return &t;
    }

    void*         alvo_ = nullptr;
    const VTable* vt_   = nullptr;

public:
    template <typename T>
        requires (!std::is_same_v<std::decay_t<T>, Funcao>)
    Funcao(T&& t)                                             // NOLINT
        : alvo_{new std::decay_t<T>{std::forward<T>(t)}},
          vt_{tabela_de<std::decay_t<T>>()} {}

    Funcao(const Funcao& o)
        : alvo_{o.alvo_ ? o.vt_->clonar(o.alvo_) : nullptr}, vt_{o.vt_} {}
    Funcao(Funcao&& o) noexcept
        : alvo_{std::exchange(o.alvo_, nullptr)}, vt_{std::exchange(o.vt_, nullptr)} {}
    Funcao& operator=(Funcao o) noexcept {          // copy-and-swap
        std::swap(alvo_, o.alvo_); std::swap(vt_, o.vt_); return *this;
    }
    ~Funcao() { if (alvo_) vt_->destruir(alvo_); }

    R operator()(Args... a) const {
        return vt_->invocar(alvo_, std::forward<Args>(a)...);
    }
};

}  // namespace v2

void demo_vtable_manual() {
    fmt::print("\n── 2.4 Vtable manual, sem herança ──\n");

    v2::Funcao<int, int> f = [](int x) { return x * x; };
    fmt::print("      v2::Funcao (vtable manual): f(9) = {}\n", f(9));

    auto g = f;                     // cópia via ponteiro `clonar`
    fmt::print("      cópia:                      g(9) = {}\n", g(9));

    fmt::print("\n      A `VTable` é um `static constexpr` por tipo T: UMA instância\n");
    fmt::print("      no binário, compartilhada por todos os objetos daquele tipo.\n");
    fmt::print("      É literalmente o que o compilador faz com `virtual` — só que\n");
    fmt::print("      aqui está escrito à mão.\n\n");

    fmt::print("      | aspecto              | virtual  | vtable manual |\n");
    fmt::print("      |----------------------|----------|---------------|\n");
    fmt::print("      | ponteiro no OBJETO   | sim(vptr)| não           |\n");
    fmt::print("      | permite SBO          | difícil  | sim           |\n");
    fmt::print("      | escolher operações   | fixo     | por chamada   |\n");
    fmt::print("      | legibilidade         | melhor   | pior          |\n\n");

    fmt::print("      Sem vptr, o objeto guardado pode ir direto para o buffer\n");
    fmt::print("      interno — é por isso que a libstdc++ escolhe esta forma.\n");
    fmt::print("      O `virtual` da v1 é mais claro e serve para aprender.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_receita();
    demo_clone();
    demo_sbo();
    demo_vtable_manual();
}

}  // namespace demo_implementacao
