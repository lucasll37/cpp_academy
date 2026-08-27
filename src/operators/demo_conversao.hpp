// =============================================================================
//  src/operators/demo_conversao.hpp
//
//  Conversões — construtores implícitos e operator T()
//  ───────────────────────────────────────────────────
//  O C++ converte tipos sozinho em muito mais lugares do que se imagina. Cada
//  conversão implícita que você habilita é uma conveniência para quem escreve
//  e uma armadilha para quem lê — porque o código deixa de dizer o que faz.
//
//  A palavra `explicit` é a única defesa, e a regra prática é simples:
//
//      Torne `explicit` TUDO, e remova o `explicit` só quando a conversão
//      for genuinamente sem perda e sem surpresa.
//
//  Tópicos:
//  • Construtor de um argumento = conversão implícita
//  • explicit em construtores e em operator T()
//  • A regra da UMA conversão definida pelo usuário
//  • explicit operator bool e o contexto "contextualmente convertível"
//  • O caso clássico: por que unique_ptr não converte para bool implicitamente
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <string>
#include <vector>

namespace demo_conversao {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Um construtor de um argumento JÁ é uma conversão
// ─────────────────────────────────────────────────────────────────────────────
namespace sem_explicit {

class Metros {
public:
    Metros(double v) : valor_(v) {}          // ⚠ implícito
    double valor() const { return valor_; }
private:
    double valor_;
};

double distancia_ate_a_parede(Metros m) { return m.valor(); }

} // namespace sem_explicit

namespace com_explicit {

class Metros {
public:
    explicit Metros(double v) : valor_(v) {}
    double valor() const { return valor_; }
private:
    double valor_;
};

double distancia_ate_a_parede(Metros m) { return m.valor(); }

} // namespace com_explicit

void demo_construtor_implicito() {
    fmt::print("\n── 3.1 Construtor de um argumento = conversão ──\n");

    fmt::print("      SEM explicit:\n");
    fmt::print("        distancia_ate_a_parede(3.5) → {:.1f}\n",
               sem_explicit::distancia_ate_a_parede(3.5));
    fmt::print("        ↑ um `double` virou `Metros` sem ninguém pedir.\n");
    fmt::print("          E se a função esperasse SEGUNDOS? Compilaria igual.\n");

    fmt::print("      COM explicit:\n");
    // com_explicit::distancia_ate_a_parede(3.5);   // ✗ não compila
    fmt::print("        distancia_ate_a_parede(3.5)          → NÃO COMPILA\n");
    fmt::print("        distancia_ate_a_parede(Metros{{3.5}}) → {:.1f}\n",
               com_explicit::distancia_ate_a_parede(com_explicit::Metros{3.5}));
    fmt::print("        ↑ a intenção fica escrita no ponto de chamada.\n");

    fmt::print("\n      O caso famoso: `std::vector<int> v(10);` cria 10 zeros,\n");
    fmt::print("      não um vetor com o elemento 10. O construtor de tamanho é\n");
    fmt::print("      `explicit` justamente para `vector<int> v = 10;` não compilar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O caminho de volta: operator T()
//
//     Um operador de conversão converte o SEU tipo em outro. Ele tem os mesmos
//     riscos do construtor implícito, e o mesmo remédio.
// ─────────────────────────────────────────────────────────────────────────────
class Temperatura {
public:
    explicit Temperatura(double celsius) : c_(celsius) {}

    // ⚠ implícito: qualquer contexto que espere double aceita Temperatura
    operator double() const { return c_; }

    double celsius()    const { return c_; }
    double fahrenheit() const { return c_ * 9.0 / 5.0 + 32.0; }

private:
    double c_;
};

class Duracao {
public:
    explicit Duracao(long segundos) : s_(segundos) {}

    // ✓ explicit: exige static_cast, e a intenção fica visível
    explicit operator long() const { return s_; }

    long segundos() const { return s_; }

private:
    long s_;
};

void demo_operator_t() {
    fmt::print("\n── 3.2 operator T() ──\n");

    Temperatura t{25.0};
    Duracao     d{3600};

    // Conversão implícita: funciona em qualquer lugar que espere double
    const double soma = t + 10.0;
    fmt::print("      Temperatura{{25}} + 10.0 = {:.1f}  (conversão implícita)\n", soma);

    // E é aí que mora o perigo:
    fmt::print("      ⚠  `t * 2` também compila, e devolve {:.1f} — um DOUBLE,\n", t * 2);
    fmt::print("         não uma Temperatura. O tipo se dissolveu silenciosamente.\n");
    fmt::print("      ⚠  pior: `t == 25` compara com double, e `Temperatura` deixou\n");
    fmt::print("         de ser um tipo forte. Todo o benefício se perdeu.\n");

    fmt::print("\n      COM explicit:\n");
    // const long x = d + 1;               // ✗ não compila
    fmt::print("        `d + 1`                    → NÃO COMPILA\n");
    fmt::print("        `static_cast<long>(d) + 1` → {}\n", static_cast<long>(d) + 1);
    fmt::print("        ↑ a saída do tipo forte fica explícita e rastreável.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. A regra da UMA conversão definida pelo usuário
//
//     Numa sequência de conversão implícita, o compilador aplica NO MÁXIMO uma
//     conversão definida pelo usuário. Conversões padrão (int→double,
//     derivado→base, array→ponteiro) não contam para esse limite.
//
//     É essa regra que impede a explosão combinatória — e também a causa de
//     muitos "por que isso não compila?".
// ─────────────────────────────────────────────────────────────────────────────
struct A { A(int) {} };
struct B { B(const A&) {} };

void recebe_b(const B&) {}

void demo_uma_conversao() {
    fmt::print("\n── 3.3 A regra da UMA conversão ──\n");

    recebe_b(B{A{1}});           // explícito: sempre funciona
    recebe_b(A{1});              // UMA conversão: A → B  ✓
    // recebe_b(1);              // DUAS: int → A → B     ✗ não compila

    fmt::print("      struct A {{ A(int); }};   struct B {{ B(const A&); }};\n");
    fmt::print("      void recebe_b(const B&);\n\n");
    fmt::print("        recebe_b(B{{A{{1}}}}) → ✓ explícito\n");
    fmt::print("        recebe_b(A{{1}})     → ✓ UMA conversão (A → B)\n");
    fmt::print("        recebe_b(1)        → ✗ DUAS conversões (int → A → B)\n");

    fmt::print("\n      Conversões PADRÃO não contam para o limite:\n");
    fmt::print("        int → double, derivado → base, array → ponteiro,\n");
    fmt::print("        T* → const T*, enum → int\n");
    fmt::print("      Então `recebe_b(A{{1.0}})` funciona: double→int é padrão,\n");
    fmt::print("      e só A→B é definida pelo usuário.\n");

    fmt::print("\n      É por isso que `std::string s = \"abc\";` funciona mas\n");
    fmt::print("      uma função que recebe `const std::string&` chamada com um\n");
    fmt::print("      tipo seu que converte para `const char*` NÃO compila.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. explicit operator bool — a exceção que confirma a regra
//
//     `explicit operator bool` é o único operador de conversão que se usa
//     `explicit` E ainda funciona implicitamente — em contextos
//     "contextualmente convertíveis para bool": if, while, &&, ||, !, ?:
//
//     Isso dá o melhor dos dois mundos: `if (obj)` funciona, mas
//     `int x = obj;` e `obj + 1` não.
// ─────────────────────────────────────────────────────────────────────────────
class Conexao {
public:
    explicit Conexao(bool aberta) : aberta_(aberta) {}

    explicit operator bool() const { return aberta_; }

private:
    bool aberta_;
};

class ConexaoRuim {
public:
    explicit ConexaoRuim(bool aberta) : aberta_(aberta) {}

    operator bool() const { return aberta_; }     // ⚠ sem explicit

private:
    bool aberta_;
};

void demo_explicit_bool() {
    fmt::print("\n── 3.4 explicit operator bool ──\n");

    Conexao c{true};
    Conexao d{false};

    // Contextos "contextualmente convertíveis": funcionam mesmo com explicit
    if (c)                      fmt::print("      if (c)          → funciona ✓\n");
    if (!d)                     fmt::print("      if (!d)         → funciona ✓\n");
    if (c && !d)                fmt::print("      if (c && !d)    → funciona ✓\n");
    fmt::print("      c ? \"aberta\" : \"fechada\" → {}\n", c ? "aberta" : "fechada");
    while (false) { }

    // Mas as conversões perigosas ficam barradas
    // int    n = c;             // ✗ não compila
    // int    s = c + 1;         // ✗ não compila
    fmt::print("      int n = c;      → NÃO COMPILA ✓\n");
    fmt::print("      c + 1;          → NÃO COMPILA ✓\n");

    // Sem explicit, tudo isso passa — e é o problema:
    ConexaoRuim r{true};
    const int absurdo = r + 41;
    fmt::print("\n      SEM explicit: `conexao + 41` = {}  ← sem sentido, e compila\n",
               absurdo);
    fmt::print("      E pior: `conexaoA == conexaoB` compararia os BOOLS,\n");
    fmt::print("      não as conexões — duas conexões fechadas seriam \"iguais\".\n");

    fmt::print("\n      Os contextos contextualmente convertíveis para bool:\n");
    fmt::print("        if  while  for(;cond;)  do-while  !  &&  ||  ?:\n");
    fmt::print("        static_assert  noexcept(expr)  explicit(expr)\n");

    fmt::print("\n      É exatamente por isso que `unique_ptr`, `optional`,\n");
    fmt::print("      `shared_ptr` e os streams usam `explicit operator bool`.\n");
    fmt::print("      Nos streams isso corrigiu um bug histórico: até o C++03\n");
    fmt::print("      eles convertiam para `void*`, e `cout << cin` compilava.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. explicit condicional (C++20)
//
//     `explicit(expr)` permite que a decisão dependa de um predicado de
//     compilação. É como a biblioteca padrão faz o construtor de `std::pair`
//     ser implícito quando os membros são implicitamente conversíveis, e
//     explícito quando não são.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
class Caixa {
public:
    // Implícito se T é conversível de int; explícito caso contrário.
    template <typename U>
    explicit(!std::is_convertible_v<U, T>)
    Caixa(U&& v) : valor_(std::forward<U>(v)) {}

    const T& valor() const { return valor_; }

private:
    T valor_;
};

void demo_explicit_condicional() {
    fmt::print("\n── 3.5 explicit condicional (C++20) ──\n");

    // double → int é conversível → construtor implícito
    Caixa<int> a = 3.9;
    fmt::print("      Caixa<int> a = 3.9;        → {} (implícito: double→int converte)\n",
               a.valor());

    // const char* → std::string é conversível → implícito
    Caixa<std::string> b = "texto";
    fmt::print("      Caixa<string> b = \"texto\"; → '{}'\n", b.valor());

    fmt::print("\n      A sintaxe:\n");
    fmt::print("        template <typename U>\n");
    fmt::print("        explicit(!std::is_convertible_v<U, T>)\n");
    fmt::print("        Caixa(U&& v);\n");
    fmt::print("      É assim que `std::pair`, `std::tuple` e `std::optional`\n");
    fmt::print("      propagam a explicitude dos tipos que embrulham.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_construtor_implicito();
    demo_operator_t();
    demo_uma_conversao();
    demo_explicit_bool();
    demo_explicit_condicional();
}

} // namespace demo_conversao
