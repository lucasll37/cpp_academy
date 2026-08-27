// =============================================================================
//  src/stl/demo_custom_container.hpp
//
//  Escrevendo um container compatível com a STL
//  ────────────────────────────────────────────
//  "Compatível com a STL" não é um selo — é um CONTRATO. Se o seu tipo o
//  cumpre, todo `<algorithm>`, todo `<ranges>`, o range-for e os adaptadores
//  passam a funcionar nele sem que você escreva uma linha a mais.
//
//  O contrato mínimo:
//    • os typedefs: value_type, size_type, iterator, const_iterator, ...
//    • begin() / end(), nas versões const e não-const
//    • um iterador que satisfaça alguma categoria
//
//  Aqui construímos um RingBuffer (fila circular de capacidade fixa) — uma
//  estrutura real, usada em áudio, redes e sistemas embarcados.
//
//  Tópicos:
//  • Os typedefs e por que os algoritmos precisam deles
//  • Um iterador random-access completo
//  • const_iterator sem duplicar código
//  • Os algoritmos da STL funcionando de graça
//  • Satisfazendo os conceitos de <ranges> (C++20)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>

namespace demo_custom_container {

// ─────────────────────────────────────────────────────────────────────────────
//  RingBuffer<T, N> — fila circular de capacidade fixa, sem alocação
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t N>
class RingBuffer {
public:
    // ── Os typedefs do contrato ──────────────────────────────────────────────
    // Não são decoração: `std::back_inserter` procura `value_type`,
    // `std::iterator_traits` procura os do iterador, e vários algoritmos
    // usam `difference_type` para a aritmética.
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;

    // ── O iterador ───────────────────────────────────────────────────────────
    // Um template sobre a constância evita escrever iterator e const_iterator
    // duas vezes — o truque padrão para não duplicar o código.
    template <bool EhConst>
    class iterador_base {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = std::conditional_t<EhConst, const T*, T*>;
        using reference         = std::conditional_t<EhConst, const T&, T&>;
        using buffer_ptr        = std::conditional_t<EhConst, const RingBuffer*, RingBuffer*>;

        iterador_base() = default;
        iterador_base(buffer_ptr b, size_type i) : buf_(b), idx_(i) {}

        // Conversão implícita iterator → const_iterator (mas não o contrário)
        operator iterador_base<true>() const { return {buf_, idx_}; }

        reference operator*()  const { return buf_->dados_[buf_->fisico(idx_)]; }
        pointer   operator->() const { return &**this; }
        reference operator[](difference_type n) const { return *(*this + n); }

        iterador_base& operator++()    { ++idx_; return *this; }
        iterador_base  operator++(int) { auto t = *this; ++idx_; return t; }
        iterador_base& operator--()    { --idx_; return *this; }
        iterador_base  operator--(int) { auto t = *this; --idx_; return t; }

        iterador_base& operator+=(difference_type n) { idx_ += static_cast<size_type>(n); return *this; }
        iterador_base& operator-=(difference_type n) { idx_ -= static_cast<size_type>(n); return *this; }

        friend iterador_base operator+(iterador_base it, difference_type n) { return it += n; }
        friend iterador_base operator+(difference_type n, iterador_base it) { return it += n; }
        friend iterador_base operator-(iterador_base it, difference_type n) { return it -= n; }

        friend difference_type operator-(const iterador_base& a, const iterador_base& b) {
            return static_cast<difference_type>(a.idx_) - static_cast<difference_type>(b.idx_);
        }

        // Em C++20, `==` e `<=>` geram os outros quatro operadores de comparação.
        friend bool operator==(const iterador_base& a, const iterador_base& b) {
            return a.idx_ == b.idx_;
        }
        friend auto operator<=>(const iterador_base& a, const iterador_base& b) {
            return a.idx_ <=> b.idx_;
        }

    private:
        buffer_ptr buf_ = nullptr;
        size_type  idx_ = 0;
    };

    using iterator       = iterador_base<false>;
    using const_iterator = iterador_base<true>;

    // ── begin/end — o que o range-for procura ────────────────────────────────
    iterator       begin()        { return {this, 0}; }
    iterator       end()          { return {this, tamanho_}; }
    const_iterator begin()  const { return {this, 0}; }
    const_iterator end()    const { return {this, tamanho_}; }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end(); }

    // ── A interface do container ─────────────────────────────────────────────
    void push(const T& v) {
        if (cheio()) {
            inicio_ = (inicio_ + 1) % N;         // sobrescreve o mais antigo
            dados_[fisico(tamanho_ - 1)] = v;
        } else {
            dados_[fisico(tamanho_)] = v;
            ++tamanho_;
        }
    }

    T pop() {
        if (vazio()) throw std::out_of_range("RingBuffer::pop em buffer vazio");
        T v = dados_[fisico(0)];
        inicio_ = (inicio_ + 1) % N;
        --tamanho_;
        return v;
    }

    reference       operator[](size_type i)       { return dados_[fisico(i)]; }
    const_reference operator[](size_type i) const { return dados_[fisico(i)]; }

    reference at(size_type i) {
        if (i >= tamanho_) throw std::out_of_range("RingBuffer::at fora do intervalo");
        return (*this)[i];
    }

    size_type size()     const noexcept { return tamanho_; }
    size_type capacity() const noexcept { return N; }
    bool      vazio()    const noexcept { return tamanho_ == 0; }
    bool      cheio()    const noexcept { return tamanho_ == N; }
    void      clear()          noexcept { inicio_ = tamanho_ = 0; }

private:
    // Traduz índice LÓGICO (0 = mais antigo) para índice FÍSICO no array
    size_type fisico(size_type logico) const noexcept { return (inicio_ + logico) % N; }

    std::array<T, N> dados_{};
    size_type        inicio_  = 0;
    size_type        tamanho_ = 0;

    template <bool> friend class iterador_base;
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. O comportamento circular
// ─────────────────────────────────────────────────────────────────────────────
void demo_comportamento() {
    fmt::print("\n── 4.1 RingBuffer — o comportamento ──\n");

    RingBuffer<int, 5> rb;

    for (int i = 1; i <= 5; ++i) rb.push(i);
    fmt::print("      após 5 pushes (cheio):     {}\n", fmt::join(rb, " "));

    rb.push(6);
    rb.push(7);
    fmt::print("      após push(6) e push(7):    {}  ← 1 e 2 foram descartados\n",
               fmt::join(rb, " "));

    fmt::print("      pop() → {}\n", rb.pop());
    fmt::print("      restante:                  {}\n", fmt::join(rb, " "));
    fmt::print("      size={} capacity={} vazio={} cheio={}\n",
               rb.size(), rb.capacity(), rb.vazio(), rb.cheio());

    fmt::print("      Nenhuma alocação em nenhum momento: os dados vivem num\n");
    fmt::print("      std::array de tamanho fixo. Ideal para tempo real.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O contrato pago: os algoritmos funcionam de graça
// ─────────────────────────────────────────────────────────────────────────────
void demo_algoritmos_de_graca() {
    fmt::print("\n── 4.2 A STL inteira funciona nele ──\n");

    RingBuffer<int, 8> rb;
    for (int v : {42, 17, 93, 8, 55, 31}) rb.push(v);

    fmt::print("      conteúdo:              {}\n", fmt::join(rb, " "));

    // range-for: precisa só de begin()/end()
    int soma = 0;
    for (int v : rb) soma += v;
    fmt::print("      range-for (soma):      {}\n", soma);

    // <algorithm>: precisa dos typedefs do iterador
    fmt::print("      std::accumulate:       {}\n",
               std::accumulate(rb.begin(), rb.end(), 0));
    fmt::print("      std::max_element:      {}\n",
               *std::max_element(rb.begin(), rb.end()));
    fmt::print("      std::count_if (> 30):  {}\n",
               std::count_if(rb.begin(), rb.end(), [](int x) { return x > 30; }));

    auto it = std::find(rb.begin(), rb.end(), 93);
    fmt::print("      std::find(93):         índice {}\n", it - rb.begin());

    // std::sort exige RANDOM ACCESS — só funciona porque implementamos
    // operator+, operator-, operator<=> e o resto da aritmética.
    std::sort(rb.begin(), rb.end());
    fmt::print("      após std::sort:        {}\n", fmt::join(rb, " "));

    // Iteradores reversos saem de graça de um bidirectional
    fmt::print("      ao contrário:          ");
    for (auto r = std::make_reverse_iterator(rb.end());
         r != std::make_reverse_iterator(rb.begin()); ++r) fmt::print("{} ", *r);
    fmt::print("\n");

    fmt::print("      Zero linhas escritas para suportar qualquer um destes.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Verificando o contrato em tempo de COMPILAÇÃO
//
//     Antes do C++20, "é um iterador random access?" era resposta de runtime
//     (leia-se: erro de template de 300 linhas). Com concepts, é um
//     static_assert que falha na hora e diz exatamente o que faltou.
// ─────────────────────────────────────────────────────────────────────────────
void demo_conceitos() {
    fmt::print("\n── 4.3 Verificando o contrato com concepts (C++20) ──\n");

    using RB  = RingBuffer<int, 8>;
    using It  = RB::iterator;
    using CIt = RB::const_iterator;

    static_assert(std::input_iterator<It>);
    static_assert(std::forward_iterator<It>);
    static_assert(std::bidirectional_iterator<It>);
    static_assert(std::random_access_iterator<It>);
    static_assert(std::random_access_iterator<CIt>);

    static_assert(std::ranges::range<RB>);
    static_assert(std::ranges::sized_range<RB>);
    static_assert(std::ranges::random_access_range<RB>);

    fmt::print("      std::random_access_iterator<iterator>       : {}\n",
               std::random_access_iterator<It>);
    fmt::print("      std::random_access_iterator<const_iterator> : {}\n",
               std::random_access_iterator<CIt>);
    fmt::print("      std::ranges::random_access_range<RingBuffer>: {}\n",
               std::ranges::random_access_range<RB>);

    // E porque é um `range`, os algoritmos de <ranges> aceitam o container
    // INTEIRO, sem begin()/end() na chamada:
    RB rb;
    for (int v : {5, 3, 9, 1, 7}) rb.push(v);

    std::ranges::sort(rb);                          // ← o container, direto
    fmt::print("      std::ranges::sort(rb):       {}\n", fmt::join(rb, " "));
    fmt::print("      std::ranges::max(rb):        {}\n", std::ranges::max(rb));

    fmt::print("      Cumprir o contrato antigo (typedefs + operadores) já\n");
    fmt::print("      satisfez os conceitos novos automaticamente.\n");
    fmt::print("      Ver src/ranges/ para o que isso destrava.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. O que ainda falta para ser um container "completo"
// ─────────────────────────────────────────────────────────────────────────────
void demo_o_que_falta() {
    fmt::print("\n── 4.4 O que um container completo ainda teria ──\n");

    fmt::print("      Implementamos o essencial. Um container de biblioteca\n");
    fmt::print("      ainda precisaria de:\n");
    fmt::print("        • allocator_type e construção allocator-aware\n");
    fmt::print("        • insert/erase com iterador, emplace com forwarding\n");
    fmt::print("        • swap() nothrow + um `friend swap` achável por ADL\n");
    fmt::print("        • operator== e operator<=> comparando elemento a elemento\n");
    fmt::print("        • dedução de template (CTAD) via deduction guides\n");
    fmt::print("        • garantias de exceção documentadas por operação\n");
    fmt::print("        • max_size(), reverse_iterator, rbegin()/rend()\n");

    fmt::print("\n      Regra prática: só escreva um container quando nenhuma\n");
    fmt::print("      combinação de containers da STL resolver. Na dúvida,\n");
    fmt::print("      componha — não reescreva.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_comportamento();
    demo_algoritmos_de_graca();
    demo_conceitos();
    demo_o_que_falta();
}

} // namespace demo_custom_container
