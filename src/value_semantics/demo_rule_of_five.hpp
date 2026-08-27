// =============================================================================
//  src/value_semantics/demo_rule_of_five.hpp
//
//  Rule of 0 / 3 / 5 — as operações especiais de um tipo
//  ─────────────────────────────────────────────────────
//  Todo tipo tem 6 operações que o compilador pode gerar sozinho:
//    destrutor, ctor de cópia, atrib. de cópia, ctor de move, atrib. de move
//    (+ ctor padrão)
//
//  Regra prática:
//    • Rule of 0 — se você não gerencia recurso bruto, NÃO declare nenhuma.
//    • Rule of 3 — se declarou uma das 3 clássicas, provavelmente precisa das 3.
//    • Rule of 5 — em C++11+, some as duas de move.
//
//  Tópicos:
//  • Contagem de operações com um tipo instrumentado
//  • Por que declarar o destrutor SUPRIME os moves
//  • `noexcept` no move e o efeito em std::vector realocando
//  • copy-and-swap
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_rule_of_five {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Um tipo que gerencia recurso bruto — precisa das 5
//
//     Buffer possui um `new[]`. Se o compilador gerasse a cópia sozinho,
//     ele copiaria o PONTEIRO — dois objetos apontando para a mesma memória,
//     double free garantido no segundo destrutor.
// ─────────────────────────────────────────────────────────────────────────────
struct Contadores {
    int ctor = 0, copia_ctor = 0, move_ctor = 0;
    int copia_atrib = 0, move_atrib = 0, dtor = 0;

    void imprimir(const char* titulo) const {
        fmt::print("  {:<28} ctor={} cópia={}/{} move={}/{} dtor={}\n",
                   titulo, ctor, copia_ctor, copia_atrib,
                   move_ctor, move_atrib, dtor);
    }
};

inline Contadores contadores;

class Buffer {
public:
    // 0) Construtor normal — adquire o recurso
    explicit Buffer(std::size_t n) : tamanho_(n), dados_(new int[n]{}) {
        ++contadores.ctor;
    }

    // 1) Destrutor — libera o recurso
    ~Buffer() {
        ++contadores.dtor;
        delete[] dados_;
    }

    // 2) Construtor de cópia — DEEP COPY: aloca memória nova e copia o conteúdo
    Buffer(const Buffer& outro) : tamanho_(outro.tamanho_), dados_(new int[outro.tamanho_]) {
        ++contadores.copia_ctor;
        for (std::size_t i = 0; i < tamanho_; ++i) dados_[i] = outro.dados_[i];
    }

    // 3) Atribuição de cópia — forma clássica
    Buffer& operator=(const Buffer& outro) {
        ++contadores.copia_atrib;
        if (this != &outro) {                 // auto-atribuição precisa ser tratada
            int* novo = new int[outro.tamanho_];   // aloca ANTES de destruir o antigo
            for (std::size_t i = 0; i < outro.tamanho_; ++i) novo[i] = outro.dados_[i];
            delete[] dados_;
            dados_   = novo;
            tamanho_ = outro.tamanho_;
        }
        return *this;
    }

    // 4) Construtor de move — ROUBA o ponteiro, não aloca nada.
    //    `noexcept` é essencial: sem ele, std::vector não usa este ctor ao realocar.
    Buffer(Buffer&& outro) noexcept
        : tamanho_(outro.tamanho_), dados_(outro.dados_) {
        ++contadores.move_ctor;
        // Deixar a origem num estado válido e destrutível:
        outro.dados_   = nullptr;
        outro.tamanho_ = 0;
    }

    // 5) Atribuição de move
    Buffer& operator=(Buffer&& outro) noexcept {
        ++contadores.move_atrib;
        if (this != &outro) {
            delete[] dados_;
            dados_   = outro.dados_;
            tamanho_ = outro.tamanho_;
            outro.dados_   = nullptr;
            outro.tamanho_ = 0;
        }
        return *this;
    }

    std::size_t tamanho() const noexcept { return tamanho_; }

    friend void trocar(Buffer& a, Buffer& b) noexcept {
        std::swap(a.tamanho_, b.tamanho_);
        std::swap(a.dados_,   b.dados_);
    }

private:
    std::size_t tamanho_ = 0;
    int*        dados_   = nullptr;
};

void demo_contagem() {
    fmt::print("\n── 2.1 Contando as operações especiais ──\n");
    contadores = {};

    {
        Buffer a(100);                    // ctor
        contadores.imprimir("Buffer a(100)");

        Buffer b = a;                     // ctor de CÓPIA (deep copy: novo new[])
        contadores.imprimir("Buffer b = a");

        Buffer c = std::move(a);          // ctor de MOVE (rouba o ponteiro)
        contadores.imprimir("Buffer c = std::move(a)");

        b = std::move(c);                 // atribuição de MOVE
        contadores.imprimir("b = std::move(c)");
    }
    contadores.imprimir("fim do escopo");
    fmt::print("  ✓ 4 construções, 4 destruições — sem leak, sem double free\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A armadilha: declarar o destrutor SUPRIME a geração dos moves
//
//     Se você escreve `~T() {}` — mesmo vazio — o compilador para de gerar
//     ctor e atribuição de move. O código continua compilando, mas todo
//     "move" silenciosamente vira CÓPIA. Bug de performance invisível.
// ─────────────────────────────────────────────────────────────────────────────
struct ComDestrutor {
    std::vector<int> dados = std::vector<int>(1000);
    ~ComDestrutor() {}                       // ← isto sozinho suprime os moves
};

struct SemDestrutor {
    std::vector<int> dados = std::vector<int>(1000);
    // Rule of 0: nada declarado, tudo gerado. Os moves existem.
};

void demo_supressao() {
    fmt::print("\n── 2.2 Um destrutor vazio custa caro ──\n");

    // is_nothrow_move_constructible é o discriminador honesto aqui:
    // is_move_constructible seria `true` nos dois casos, porque um `const T&`
    // também aceita rvalue — a cópia se disfarça de move.
    fmt::print("  ComDestrutor  — move real disponível? {}\n",
               std::is_nothrow_move_constructible_v<ComDestrutor>
                   ? "sim" : "NÃO — cai silenciosamente na cópia");
    fmt::print("  SemDestrutor  — move real disponível? {}\n",
               std::is_nothrow_move_constructible_v<SemDestrutor> ? "sim" : "não");

    fmt::print("  Repare que os dois compilam e rodam igual — a diferença é só\n");
    fmt::print("  de performance, e nenhum warning avisa.\n");
    fmt::print("  Regra: se declarou destrutor, declare também os 4 restantes\n");
    fmt::print("         (ou use `= default` explicitamente).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. noexcept no move e a realocação do std::vector
//
//     std::vector precisa da garantia FORTE de exceção ao realocar: se algo
//     falhar no meio, o vetor original tem que continuar íntegro.
//     • move noexcept    → o vector MOVE os elementos (rápido)
//     • move que pode lançar → o vector COPIA os elementos (lento, mas seguro)
//     É `std::move_if_noexcept` decidindo por baixo dos panos.
// ─────────────────────────────────────────────────────────────────────────────
struct MoveNoexcept {
    std::vector<int> d = std::vector<int>(64);
    MoveNoexcept() = default;
    MoveNoexcept(const MoveNoexcept& o) : d(o.d) { ++copias; }
    MoveNoexcept(MoveNoexcept&& o) noexcept : d(std::move(o.d)) { ++moves; }
    static inline int copias = 0, moves = 0;
};

struct MovePodeLancar {
    std::vector<int> d = std::vector<int>(64);
    MovePodeLancar() = default;
    MovePodeLancar(const MovePodeLancar& o) : d(o.d) { ++copias; }
    MovePodeLancar(MovePodeLancar&& o) : d(std::move(o.d)) { ++moves; }  // sem noexcept
    static inline int copias = 0, moves = 0;
};

void demo_noexcept_vector() {
    fmt::print("\n── 2.3 noexcept decide se o vector move ou copia ──\n");

    {
        std::vector<MoveNoexcept> v;
        for (int i = 0; i < 8; ++i) v.emplace_back();   // força realocações
        fmt::print("  MoveNoexcept    → {} moves, {} cópias\n",
                   MoveNoexcept::moves, MoveNoexcept::copias);
    }
    {
        std::vector<MovePodeLancar> v;
        for (int i = 0; i < 8; ++i) v.emplace_back();
        fmt::print("  MovePodeLancar  → {} moves, {} cópias\n",
                   MovePodeLancar::moves, MovePodeLancar::copias);
    }
    fmt::print("  ⚠  esquecer o `noexcept` no move é a regressão de performance\n");
    fmt::print("     mais comum e mais silenciosa do C++ moderno.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. copy-and-swap — UMA atribuição que cobre cópia E move
//
//     Cofre& operator=(Cofre outro)   ← recebe POR VALOR
//
//     • chamada com lvalue → o parâmetro é construído por CÓPIA
//     • chamada com rvalue → o parâmetro é construído por MOVE
//     Um único corpo atende os dois casos. E como a cópia acontece ANTES de
//     tocar em `*this`, se ela lançar, o objeto original fica intacto —
//     é a garantia FORTE de exceção saindo de graça.
//
//     ⚠  Só declare a versão por valor. Ter `operator=(T)` e `operator=(T&&)`
//        ao mesmo tempo é AMBÍGUO para argumentos rvalue e não compila.
// ─────────────────────────────────────────────────────────────────────────────
class Cofre {
public:
    explicit Cofre(std::size_t n) : tamanho_(n), dados_(new int[n]{}) {}
    ~Cofre() { delete[] dados_; }

    Cofre(const Cofre& o) : tamanho_(o.tamanho_), dados_(new int[o.tamanho_]) {
        fmt::print("    [Cofre] ctor de cópia\n");
        for (std::size_t i = 0; i < tamanho_; ++i) dados_[i] = o.dados_[i];
    }

    Cofre(Cofre&& o) noexcept : tamanho_(o.tamanho_), dados_(o.dados_) {
        fmt::print("    [Cofre] ctor de move\n");
        o.dados_ = nullptr;
        o.tamanho_ = 0;
    }

    // A única atribuição declarada — cobre os dois casos.
    Cofre& operator=(Cofre outro) noexcept {
        fmt::print("    [Cofre] operator= (por valor) + swap\n");
        trocar(*this, outro);
        return *this;
    }

    friend void trocar(Cofre& a, Cofre& b) noexcept {
        std::swap(a.tamanho_, b.tamanho_);
        std::swap(a.dados_,   b.dados_);
    }

private:
    std::size_t tamanho_ = 0;
    int*        dados_   = nullptr;
};

void demo_copy_and_swap() {
    fmt::print("\n── 2.4 copy-and-swap ──\n");

    Cofre a(10);
    Cofre b(20);

    fmt::print("  a = b;            // lvalue\n");
    a = b;                       // parâmetro construído por CÓPIA

    fmt::print("  a = Cofre(30);    // rvalue\n");
    a = Cofre(30);               // parâmetro construído por MOVE

    fmt::print("  Vantagens: 1 função para os 2 casos, garantia forte de exceção,\n");
    fmt::print("             auto-atribuição correta de graça.\n");
    fmt::print("  Custo:     sempre constrói o parâmetro — nem sempre é o ótimo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Rule of 0 — o objetivo final
//
//     Se cada membro já gerencia a própria vida (vector, string, unique_ptr),
//     o tipo composto não declara NENHUMA das 6 operações. Zero código,
//     zero bug de lifetime, todas as operações corretas e ótimas.
// ─────────────────────────────────────────────────────────────────────────────
struct Pessoa {                    // Rule of 0 na prática
    std::string      nome;
    std::vector<int> notas;
    // sem destrutor, sem cópia, sem move — o compilador acerta tudo
};

void demo_rule_of_zero() {
    fmt::print("\n── 2.5 Rule of 0 — o alvo a mirar ──\n");

    static_assert(std::is_copy_constructible_v<Pessoa>);
    static_assert(std::is_nothrow_move_constructible_v<Pessoa>);

    Pessoa p{"Ada", {10, 9, 10}};
    Pessoa q = std::move(p);       // move dos membros, gerado pelo compilador

    fmt::print("  Pessoa não declara nenhuma operação especial e ainda assim:\n");
    fmt::print("    copiável           : {}\n", std::is_copy_constructible_v<Pessoa>);
    fmt::print("    move sem lançar    : {}\n", std::is_nothrow_move_constructible_v<Pessoa>);
    fmt::print("    q.nome após o move : '{}'\n", q.nome);
    fmt::print("  Prefira SEMPRE a Rule of 0. As Rules 3/5 são para quem escreve\n");
    fmt::print("  o tipo que gerencia o recurso bruto — e isso deve ser raro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_contagem();
    demo_supressao();
    demo_noexcept_vector();
    demo_copy_and_swap();
    demo_rule_of_zero();
}

} // namespace demo_rule_of_five
