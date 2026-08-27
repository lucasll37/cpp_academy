// =============================================================================
//  src/operators/demo_chamada.hpp
//
//  operator(), operator[] e operator->
//  ───────────────────────────────────
//  Três operadores que precisam ser MEMBROS e que, juntos, permitem que um
//  tipo seu se pareça com uma função, um array ou um ponteiro.
//
//  O `operator()` é o mais importante dos três: ele é o que torna um objeto
//  CHAMÁVEL, e é a base de todo o código genérico da STL. Um lambda é
//  literalmente uma classe com `operator()` — ver src/lambdas/.
//
//  Tópicos:
//  • Functors: objetos que se comportam como função (e por que superam ponteiros)
//  • operator[] e o problema do índice multidimensional
//  • Subscript multidimensional (C++23)
//  • operator-> e o drill-down: proxies e wrappers
//  • static operator() e operator[] (C++23)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace demo_chamada {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Functors — e por que eles batem ponteiros de função
//
//     A diferença decisiva: um functor tem TIPO próprio, então o compilador
//     sabe exatamente qual código chamar e pode INLINAR. Um ponteiro de função
//     é um valor de runtime — nada a inlinar.
// ─────────────────────────────────────────────────────────────────────────────
struct MaiorQue {
    int limite;
    bool operator()(int x) const { return x > limite; }
};

// Um functor com ESTADO MUTÁVEL: acumula entre chamadas
class Contador {
public:
    int operator()() { return ++n_; }
    int total() const { return n_; }
private:
    int n_ = 0;
};

bool maior_que_10(int x) { return x > 10; }        // função livre, para comparar

void demo_functors() {
    fmt::print("\n── 4.1 Functors ──\n");

    const std::vector<int> v{5, 12, 8, 20, 3, 15};

    // O functor carrega o parâmetro consigo — um ponteiro de função não pode
    fmt::print("      count_if(MaiorQue{{10}})  = {}\n",
               std::count_if(v.begin(), v.end(), MaiorQue{10}));
    fmt::print("      count_if(MaiorQue{{5}})   = {}  ← MESMO tipo, estado diferente\n",
               std::count_if(v.begin(), v.end(), MaiorQue{5}));
    fmt::print("      count_if(maior_que_10)  = {}  ← o 10 está CHUMBADO no código\n",
               std::count_if(v.begin(), v.end(), maior_que_10));

    // Estado mutável entre chamadas
    Contador c;
    fmt::print("      contador: {} {} {} → total {}\n", c(), c(), c(), c.total());

    fmt::print("\n      | forma            | estado | inlinável | tipo próprio |\n");
    fmt::print("      |------------------|--------|-----------|--------------|\n");
    fmt::print("      | ponteiro p/ função| não    | NÃO       | não          |\n");
    fmt::print("      | functor          | SIM    | SIM       | SIM          |\n");
    fmt::print("      | lambda           | SIM    | SIM       | SIM (anônimo)|\n");
    fmt::print("      | std::function    | SIM    | não       | apagado      |\n");

    fmt::print("\n      Um lambda É um functor: o compilador gera uma classe\n");
    fmt::print("      anônima com `operator() const`. As capturas viram membros.\n");
    fmt::print("      Ver src/lambdas/ para o desdobramento completo.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. operator[] e o problema do índice multidimensional
//
//     Até o C++23, `operator[]` aceitava EXATAMENTE um argumento. Uma matriz
//     tinha três saídas, todas com defeito.
// ─────────────────────────────────────────────────────────────────────────────
class MatrizClassica {
public:
    MatrizClassica(std::size_t l, std::size_t c)
        : linhas_(l), colunas_(c), dados_(l * c, 0.0) {}

    // Saída (a): operator() com dois argumentos. Funciona, mas `m(i,j)` parece
    // uma chamada de função, não um acesso.
    double& operator()(std::size_t i, std::size_t j) {
        return dados_[i * colunas_ + j];
    }
    double operator()(std::size_t i, std::size_t j) const {
        return dados_[i * colunas_ + j];
    }

    // Saída (b): operator[] devolvendo um PROXY que também tem operator[].
    // Permite `m[i][j]`, ao custo de uma classe extra.
    class Linha {
    public:
        Linha(double* inicio, std::size_t n) : inicio_(inicio), n_(n) {}
        double& operator[](std::size_t j) { return inicio_[j]; }
    private:
        double*     inicio_;
        std::size_t n_;
    };

    Linha operator[](std::size_t i) { return {dados_.data() + i * colunas_, colunas_}; }

    std::size_t linhas() const { return linhas_; }
    std::size_t colunas() const { return colunas_; }

private:
    std::size_t         linhas_, colunas_;
    std::vector<double> dados_;
};

void demo_indice() {
    fmt::print("\n── 4.2 operator[] e o índice multidimensional ──\n");

    MatrizClassica m{2, 3};

    m(0, 0) = 1; m(0, 1) = 2; m(0, 2) = 3;
    m[1][0]  = 4; m[1][1] = 5; m[1][2] = 6;      // via proxy

    fmt::print("      m(i,j) — operator() com 2 args:\n");
    for (std::size_t i = 0; i < m.linhas(); ++i) {
        fmt::print("        ");
        for (std::size_t j = 0; j < m.colunas(); ++j) fmt::print("{:4.0f}", m(i, j));
        fmt::print("\n");
    }
    fmt::print("      m[i][j] — operator[] devolvendo proxy: m[1][2] = {:.0f}\n", m[1][2]);

    fmt::print("\n      As três saídas pré-C++23, e o defeito de cada uma:\n");
    fmt::print("        (a) m(i,j)   → parece chamada de função, não indexação\n");
    fmt::print("        (b) m[i][j]  → exige uma classe proxy por dimensão, e o\n");
    fmt::print("                       proxy pode ficar pendurado\n");
    fmt::print("        (c) m[{{i,j}}]  → precisa construir um par a cada acesso\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Subscript multidimensional (C++23)
//
//     A partir do C++23, `operator[]` aceita qualquer número de argumentos.
//     A saída (a) e a (b) deixam de ser necessárias.
// ─────────────────────────────────────────────────────────────────────────────
class Matriz3D {
public:
    Matriz3D(std::size_t x, std::size_t y, std::size_t z)
        : x_(x), y_(y), z_(z), dados_(x * y * z, 0.0) {}

    // C++23: múltiplos parâmetros no operator[]
    double& operator[](std::size_t i, std::size_t j, std::size_t k) {
        return dados_[(i * y_ + j) * z_ + k];
    }
    double operator[](std::size_t i, std::size_t j, std::size_t k) const {
        return dados_[(i * y_ + j) * z_ + k];
    }

private:
    std::size_t         x_, y_, z_;
    std::vector<double> dados_;
};

void demo_multidim() {
    fmt::print("\n── 4.3 Subscript multidimensional (C++23) ──\n");

    Matriz3D vol{2, 2, 2};
    vol[0, 0, 0] = 1.0;
    vol[1, 1, 1] = 8.0;
    vol[0, 1, 0] = 3.0;

    fmt::print("      vol[0,0,0] = {:.0f}\n", vol[0, 0, 0]);
    fmt::print("      vol[0,1,0] = {:.0f}\n", vol[0, 1, 0]);
    fmt::print("      vol[1,1,1] = {:.0f}\n", vol[1, 1, 1]);

    fmt::print("\n      Sem proxy, sem par, e a sintaxe diz \"indexação\".\n");
    fmt::print("      ⚠  cuidado com a mudança de significado: em C++20,\n");
    fmt::print("         `a[i, j]` era o operador VÍRGULA (avaliava i, descartava,\n");
    fmt::print("         e indexava por j). O C++20 deprecou esse uso e o C++23\n");
    fmt::print("         reciclou a sintaxe. Código antigo com `a[i, j]` muda de\n");
    fmt::print("         comportamento — o GCC avisa com -Wcomma-subscript.\n");

    fmt::print("\n      É esta feature que sustenta o `std::mdspan` do C++23\n");
    fmt::print("      (que o GCC 13 ainda não tem — ver src/ROADMAP.md).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. operator-> e o drill-down
//
//     `operator->` tem uma regra única na linguagem: se o que ele devolve não
//     é um ponteiro cru, o compilador chama `operator->` DE NOVO no resultado,
//     recursivamente, até chegar a um ponteiro. É o "drill-down".
//
//     É isso que permite a um wrapper repassar o acesso ao objeto embrulhado
//     sem escrever um método por membro.
// ─────────────────────────────────────────────────────────────────────────────
struct Documento {
    std::string titulo;
    int         paginas;

    void imprimir() const { fmt::print("        '{}' ({} páginas)\n", titulo, paginas); }
};

// Um wrapper que registra cada acesso — o padrão Proxy, em miniatura
template <typename T>
class Registrador {
public:
    explicit Registrador(T* alvo) : alvo_(alvo) {}

    // Devolve um PROXY temporário; o drill-down continua a partir dele
    class Acesso {
    public:
        Acesso(T* alvo, int& contador) : alvo_(alvo), contador_(contador) {
            ++contador_;
        }
        T* operator->() const { return alvo_; }     // aqui SIM devolve ponteiro
    private:
        T*   alvo_;
        int& contador_;
    };

    Acesso operator->() const { return {alvo_, acessos_}; }

    int acessos() const { return acessos_; }

private:
    T*          alvo_;
    mutable int acessos_ = 0;
};

void demo_seta() {
    fmt::print("\n── 4.4 operator-> e o drill-down ──\n");

    Documento doc{"Relatório Anual", 42};
    Registrador<Documento> reg{&doc};

    fmt::print("      acessando através do Registrador:\n");
    reg->imprimir();
    fmt::print("      título via reg: '{}'\n", reg->titulo);
    fmt::print("      páginas via reg: {}\n", reg->paginas);
    fmt::print("      acessos registrados: {}\n", reg.acessos());

    fmt::print("\n      A regra do drill-down:\n");
    fmt::print("        reg->titulo\n");
    fmt::print("          → reg.operator->()          devolve Acesso (não é ponteiro)\n");
    fmt::print("          → Acesso.operator->()       devolve Documento* (é ponteiro!)\n");
    fmt::print("          → Documento*->titulo        acesso final\n");
    fmt::print("      O compilador aplica `operator->` repetidamente até chegar\n");
    fmt::print("      a um ponteiro cru. Nenhum outro operador se comporta assim.\n");

    fmt::print("\n      É o mecanismo por trás de `unique_ptr`, `shared_ptr`,\n");
    fmt::print("      `optional`, dos iteradores, e de qualquer proxy que precise\n");
    fmt::print("      repassar o acesso sem replicar a interface.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. static operator() e static operator[] (C++23)
//
//     Um functor SEM ESTADO não precisa do `this`. Marcá-lo `static` elimina o
//     parâmetro implícito e permite chamar sem instância.
// ─────────────────────────────────────────────────────────────────────────────
struct EhPar {
    static bool operator()(int x) { return x % 2 == 0; }   // C++23
};

struct EhParClassico {
    bool operator()(int x) const { return x % 2 == 0; }
};

void demo_static_call() {
    fmt::print("\n── 4.5 static operator() (C++23) ──\n");

    const std::vector<int> v{1, 2, 3, 4, 5, 6};

    fmt::print("      count_if com static operator() = {}\n",
               std::count_if(v.begin(), v.end(), EhPar{}));
    fmt::print("      chamada direta, sem instância: EhPar{{}}(4) = {}\n", EhPar{}(4));

    fmt::print("      sizeof(EhPar)         = {}\n", sizeof(EhPar));
    fmt::print("      sizeof(EhParClassico) = {}  (classes vazias têm sizeof 1)\n",
               sizeof(EhParClassico));

    fmt::print("\n      O ganho não é o sizeof — é a ausência do parâmetro `this`\n");
    fmt::print("      implícito na chamada. Em código genérico muito quente,\n");
    fmt::print("      isso remove um argumento por chamada.\n");
    fmt::print("      É por isso que os adaptadores de `<ranges>` do C++23 usam\n");
    fmt::print("      `static operator()`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_functors();
    demo_indice();
    demo_multidim();
    demo_seta();
    demo_static_call();
}

} // namespace demo_chamada
