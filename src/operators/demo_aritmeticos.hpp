// =============================================================================
//  src/operators/demo_aritmeticos.hpp
//
//  Sobrecarga de operadores — a forma canônica
//  ───────────────────────────────────────────
//  Sobrecarregar operadores é a maneira de fazer um tipo seu se comportar como
//  um tipo embutido. Feito bem, `a + b * c` funciona e significa o óbvio.
//  Feito mal, é a origem de código ilegível — e o C++ tem má fama justamente
//  por permitir os dois.
//
//  A regra que resolve 90% das decisões:
//
//      NÃO SURPREENDA. Se `+` não for comutativo, associativo e barato,
//      escreva um método com nome.
//
//  Tópicos:
//  • Membro ou função livre — e a assimetria que decide
//  • Hidden friends: por que definir dentro da classe
//  • A forma canônica: `+` implementado em termos de `+=`
//  • Operadores que quase nunca devem ser sobrecarregados
//  • Imprimir o próprio tipo: `operator<<` e `fmt::formatter`
//  • `++` e `--`: o parâmetro `int` que distingue prefixo de posfixo
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/format.h>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace demo_aritmeticos {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Membro ou função livre?
//
//     A diferença aparece na CONVERSÃO IMPLÍCITA do operando da esquerda:
//     um operador MEMBRO não converte o `this`, uma função LIVRE converte os
//     dois operandos.
// ─────────────────────────────────────────────────────────────────────────────
class DinheiroMembro {
public:
    DinheiroMembro(double v) : centavos_(static_cast<long>(v * 100)) {}   // implícito

    // Operador MEMBRO: o operando da esquerda TEM que ser DinheiroMembro
    DinheiroMembro operator+(const DinheiroMembro& o) const {
        return DinheiroMembro{(centavos_ + o.centavos_) / 100.0};
    }
    double valor() const { return centavos_ / 100.0; }

private:
    long centavos_;
};

class DinheiroLivre {
public:
    DinheiroLivre(double v) : centavos_(static_cast<long>(v * 100)) {}
    double valor() const { return centavos_ / 100.0; }
    long centavos() const { return centavos_; }

    // Função LIVRE declarada como friend dentro da classe (hidden friend).
    // Aqui os DOIS operandos passam por conversão implícita.
    friend DinheiroLivre operator+(const DinheiroLivre& a, const DinheiroLivre& b) {
        return DinheiroLivre{(a.centavos_ + b.centavos_) / 100.0};
    }

private:
    long centavos_;
};

void demo_membro_vs_livre() {
    fmt::print("\n── 1.1 Membro ou função livre? ──\n");

    DinheiroMembro m{10.0};
    DinheiroLivre  l{10.0};

    fmt::print("      com operador MEMBRO:\n");
    fmt::print("        m + 5.0   → {:.2f}  ✓ (o double vira DinheiroMembro)\n",
               (m + 5.0).valor());
    // 5.0 + m;                    // ✗ não compila: double não tem operator+(DinheiroMembro)
    fmt::print("        5.0 + m   → NÃO COMPILA (o `this` não converte)\n");

    fmt::print("      com função LIVRE:\n");
    fmt::print("        l + 5.0   → {:.2f}  ✓\n", (l + 5.0).valor());
    fmt::print("        5.0 + l   → {:.2f}  ✓ (os DOIS operandos convertem)\n",
               (5.0 + l).valor());

    fmt::print("\n      | operador               | forma            |\n");
    fmt::print("      |------------------------|------------------|\n");
    fmt::print("      | simétrico (+ - * / ==) | função LIVRE     |\n");
    fmt::print("      | modifica o objeto (+=) | MEMBRO           |\n");
    fmt::print("      | = [] () -> ->*         | MEMBRO (exigido) |\n");
    fmt::print("      | << >> (streams)        | LIVRE (exigido)  |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Hidden friends — por que definir DENTRO da classe
//
//     Um `friend` definido no corpo da classe não é visível por busca de nome
//     comum: só por ADL, a partir dos seus argumentos. Isso tem três efeitos:
//       • não polui o namespace
//       • acelera a resolução de overload (menos candidatos a considerar)
//       • impede que uma conversão implícita traga a função para um contexto
//         onde ela não faz sentido
// ─────────────────────────────────────────────────────────────────────────────
namespace exemplo_adl {

struct Vetor2 {
    double x{}, y{};

    // Hidden friend: só encontrável por ADL sobre Vetor2
    friend Vetor2 operator+(const Vetor2& a, const Vetor2& b) {
        return {a.x + b.x, a.y + b.y};
    }
    friend Vetor2 operator*(const Vetor2& v, double k) { return {v.x * k, v.y * k}; }
    friend Vetor2 operator*(double k, const Vetor2& v) { return v * k; }   // simetria
};

} // namespace exemplo_adl

void demo_hidden_friends() {
    fmt::print("\n── 1.2 Hidden friends ──\n");

    exemplo_adl::Vetor2 a{1, 2}, b{3, 4};
    auto soma = a + b;                      // achado por ADL, sem `using`
    auto escalado = 2.0 * a;

    fmt::print("      {{1,2}} + {{3,4}}  = {{{}, {}}}\n", soma.x, soma.y);
    fmt::print("      2.0 * {{1,2}}    = {{{}, {}}}\n", escalado.x, escalado.y);

    fmt::print("      Nenhum `using namespace` foi necessário: o ADL encontra\n");
    fmt::print("      `operator+` porque um dos argumentos é `exemplo_adl::Vetor2`.\n");

    fmt::print("\n      O que se ganha:\n");
    fmt::print("        • o namespace não fica poluído com N operadores\n");
    fmt::print("        • a resolução de overload considera menos candidatos\n");
    fmt::print("          (importa em projeto grande: é tempo de compilação)\n");
    fmt::print("        • os dois operandos ainda convertem implicitamente\n");
    fmt::print("      Recomendação: declare operadores livres SEMPRE como hidden\n");
    fmt::print("      friends, a menos que precise deles fora do ADL.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. A forma canônica
//
//     Implemente a versão que MODIFICA (`+=`) primeiro, e derive a que cria
//     (`+`) dela. Isso evita duplicar a lógica e garante que as duas concordem.
//
//     A assinatura do `+` livre recebe o primeiro operando POR VALOR: assim o
//     parâmetro já é a cópia que vamos modificar e devolver — e se o chamador
//     passou um rvalue, ele é movido em vez de copiado.
// ─────────────────────────────────────────────────────────────────────────────
class Polinomio {
public:
    Polinomio() = default;
    explicit Polinomio(std::vector<double> coefs) : coefs_(std::move(coefs)) {}

    // 1) A operação que MODIFICA — aqui mora a lógica, e é membro
    Polinomio& operator+=(const Polinomio& o) {
        if (o.coefs_.size() > coefs_.size()) coefs_.resize(o.coefs_.size(), 0.0);
        for (std::size_t i = 0; i < o.coefs_.size(); ++i) coefs_[i] += o.coefs_[i];
        return *this;                        // devolve referência, para encadear
    }

    Polinomio& operator*=(double k) {
        for (double& c : coefs_) c *= k;
        return *this;
    }

    // 2) As que CRIAM — derivadas das acima, como hidden friends.
    //    `a` por VALOR: é a cópia de trabalho, e aceita move de um rvalue.
    friend Polinomio operator+(Polinomio a, const Polinomio& b) { return a += b; }
    friend Polinomio operator*(Polinomio a, double k)           { return a *= k; }
    friend Polinomio operator*(double k, Polinomio a)           { return a *= k; }

    // 3) Unário: distinto do binário pela ausência de segundo parâmetro
    Polinomio operator-() const {
        Polinomio r = *this;
        return r *= -1.0;
    }

    double avaliar(double x) const {
        double r = 0, potencia = 1;
        for (double c : coefs_) { r += c * potencia; potencia *= x; }
        return r;
    }

    std::string str() const {
        if (coefs_.empty()) return "0";
        std::string s;
        for (std::size_t i = coefs_.size(); i-- > 0;) {
            if (coefs_[i] == 0) continue;
            if (!s.empty()) s += coefs_[i] > 0 ? " + " : " - ";
            else if (coefs_[i] < 0) s += "-";
            const double a = coefs_[i] < 0 ? -coefs_[i] : coefs_[i];
            s += (i == 0 || a != 1) ? fmt::format("{:g}", a) : "";
            if (i >= 1) s += "x";
            if (i >= 2) s += fmt::format("^{}", i);
        }
        return s.empty() ? "0" : s;
    }

private:
    std::vector<double> coefs_;              // coefs_[i] multiplica x^i
};

void demo_forma_canonica() {
    fmt::print("\n── 1.3 A forma canônica ──\n");

    Polinomio p{{1, 2, 3}};                  // 3x² + 2x + 1
    Polinomio q{{0, 1}};                     // x

    fmt::print("      p        = {}\n", p.str());
    fmt::print("      q        = {}\n", q.str());
    fmt::print("      p + q    = {}\n", (p + q).str());
    fmt::print("      p * 2    = {}\n", (p * 2).str());
    fmt::print("      2 * p    = {}\n", (2 * p).str());
    fmt::print("      -p       = {}\n", (-p).str());
    fmt::print("      p(2)     = {:g}\n", p.avaliar(2));

    // += encadeável porque devolve referência
    Polinomio acc;
    acc += p; acc += q; acc += q;
    fmt::print("      acumulado = {}\n", acc.str());

    fmt::print("\n      A receita, em três linhas:\n");
    fmt::print("        Tipo& operator+=(const Tipo& o);                 // membro, a lógica\n");
    fmt::print("        friend Tipo operator+(Tipo a, const Tipo& b)     // livre, por VALOR\n");
    fmt::print("            {{ return a += b; }}\n");
    fmt::print("      O `a` por valor é a cópia de trabalho — e se o chamador\n");
    fmt::print("      passou um temporário, ele é MOVIDO (src/value_semantics/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Os operadores que quase nunca devem ser sobrecarregados
// ─────────────────────────────────────────────────────────────────────────────
void demo_nao_sobrecarregue() {
    fmt::print("\n── 1.4 O que NÃO sobrecarregar ──\n");

    fmt::print("      | operador  | por quê não                                |\n");
    fmt::print("      |-----------|--------------------------------------------|\n");
    fmt::print("      | &&  ||    | perde o curto-circuito — vira chamada normal|\n");
    fmt::print("      |           | e AMBOS os lados são avaliados             |\n");
    fmt::print("      | ,         | perde a ordem de avaliação garantida       |\n");
    fmt::print("      | &  (unário)| quebra genéricos que esperam o endereço   |\n");
    fmt::print("      | ->*       | ninguém entende, ninguém usa               |\n");
    fmt::print("      | new/delete| só se você está escrevendo um alocador     |\n");
    fmt::print("      |           | (e aí veja src/memory/)                    |\n");

    fmt::print("\n      E os que são tecnicamente possíveis, mas quase sempre\n");
    fmt::print("      um erro de design:\n");
    fmt::print("        • `+` que não é comutativo (string + é a exceção aceita)\n");
    fmt::print("        • `<<` que não é saída de stream (Qt e Boost fizeram, e\n");
    fmt::print("          gerou confusão por décadas)\n");
    fmt::print("        • `%` para formatação, `^` para potência — o leitor\n");
    fmt::print("          espera resto e XOR, e a PRECEDÊNCIA não muda\n");

    fmt::print("\n      O teste decisivo: se alguém que nunca viu o seu tipo\n");
    fmt::print("      não consegue prever o que `a op b` faz, use um método\n");
    fmt::print("      com nome. `a.concatenar(b)` nunca confundiu ninguém.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Imprimir o próprio tipo, e os operadores de incremento
//
//     São os dois primeiros operadores que qualquer pessoa quer sobrecarregar,
//     e cada um tem uma regra que não é óbvia:
//
//       • `operator<<` TEM que ser função livre. O operando da esquerda é o
//         stream, não o seu tipo — e você não pode acrescentar um membro a
//         `std::ostream`. Ele recebe e devolve `std::ostream&` para permitir
//         o encadeamento `os << a << b`.
//
//       • `++` prefixo e `++` posfixo têm a MESMA cara. A linguagem os
//         distingue por um parâmetro `int` que não é usado para nada: quem
//         tem o `int` é o posfixo.
// ─────────────────────────────────────────────────────────────────────────────
class Contador {
public:
    constexpr explicit Contador(int v = 0) : v_(v) {}

    constexpr int valor() const { return v_; }

    // ++c — prefixo: incrementa e devolve REFERÊNCIA ao próprio objeto.
    Contador& operator++() { ++v_; return *this; }
    Contador& operator--() { --v_; return *this; }

    // c++ — posfixo: o parâmetro `int` é só o marcador que diz "sou o
    //       posfixo". Ele devolve uma CÓPIA do estado anterior — e é por
    //       isso que `c++` custa mais que `++c`.
    Contador operator++(int) { Contador anterior = *this; ++v_; return anterior; }
    Contador operator--(int) { Contador anterior = *this; --v_; return anterior; }

    // A forma canônica de `operator<<`: hidden friend, `std::ostream&` nos
    // dois lados, e nada de `std::endl` dentro (quem imprime decide isso).
    friend std::ostream& operator<<(std::ostream& os, const Contador& c) {
        return os << "Contador(" << c.v_ << ')';
    }

private:
    int v_;
};

} // namespace demo_aritmeticos

// A alternativa moderna ao `operator<<`: especializar o formatter. Ele precisa
// morar no namespace da BIBLIOTECA (fmt::, ou std:: para `std::format`), não no
// seu — por isso o namespace do demo é fechado aqui e reaberto embaixo.
template <>
struct fmt::formatter<demo_aritmeticos::Contador> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    auto format(const demo_aritmeticos::Contador& c, fmt::format_context& ctx) const {
        return fmt::format_to(ctx.out(), "Contador({})", c.valor());
    }
};

namespace demo_aritmeticos {

void demo_impressao_e_incremento() {
    fmt::print("\n── 1.5 Imprimir o próprio tipo, e ++/-- ──\n");

    const Contador c{7};

    // Via operator<<: o tipo entra em qualquer std::ostream (cout, ofstream,
    // ostringstream). Aqui vai para um ostringstream só para capturar o texto.
    std::ostringstream os;
    os << c;
    fmt::print("      via operator<<     : {}\n", os.str());

    // Via fmt::formatter: o mesmo tipo agora funciona em fmt::print/format.
    fmt::print("      via fmt::formatter : {}\n", c);

    fmt::print("\n      operator<< é obrigatoriamente função LIVRE: o operando\n");
    fmt::print("      da esquerda é o stream, e não dá para acrescentar um\n");
    fmt::print("      membro a std::ostream. A assinatura canônica é\n");
    fmt::print("        std::ostream& operator<<(std::ostream& os, const T& v);\n");
    fmt::print("      Devolver `os` é o que permite `os << a << b << c`.\n");

    fmt::print("\n      prefixo vs posfixo:\n");
    Contador p{0};
    Contador devolvido_prefixo = ++p;      // incrementa, devolve o novo estado
    fmt::print("        ++p  → devolveu {}, p agora é {}\n",
               devolvido_prefixo.valor(), p.valor());

    Contador q{0};
    Contador devolvido_posfixo = q++;      // devolve o estado ANTERIOR
    fmt::print("        q++  → devolveu {}, q agora é {}\n",
               devolvido_posfixo.valor(), q.valor());

    fmt::print("\n      As duas assinaturas, lado a lado:\n");
    fmt::print("        Tipo& operator++();       // prefixo — devolve referência\n");
    fmt::print("        Tipo  operator++(int);    // posfixo — o `int` é um marcador\n");
    fmt::print("                                  //   sem nome e sem uso; devolve CÓPIA\n");
    fmt::print("      Regra: escreva o prefixo primeiro e derive o posfixo dele.\n");
    fmt::print("      Regra: em laços, prefira `++i` — `i++` fabrica uma cópia\n");
    fmt::print("      que ninguém lê. Em `int` isso some na otimização; num\n");
    fmt::print("      iterador de árvore ou numa classe grande, não some.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_membro_vs_livre();
    demo_hidden_friends();
    demo_forma_canonica();
    demo_nao_sobrecarregue();
    demo_impressao_e_incremento();
}

} // namespace demo_aritmeticos
