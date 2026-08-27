// =============================================================================
//  src/syntax/demo_generico.hpp
//
//  Sintaxe genérica e de erros — templates, concepts e exceções
//  ───────────────────────────────────────────────────────────
//  Duas construções que mudam a FORMA do código, não só o seu conteúdo:
//
//      template <typename T>        →  o compilador escreve uma versão por tipo
//      try / throw / catch          →  um segundo caminho de saída da função
//
//  Este arquivo cobre a SINTAXE de ambas. A semântica profunda — instanciação,
//  SFINAE, subsumição de concepts, garantias de exceção — está nos subprojetos
//  dedicados: src/templates, src/concepts e src/errors.
//
//  Tópicos:
//  • Template de função: `typename` vs `class`, dedução, argumento explícito
//  • Template de classe e CTAD (C++17)
//  • Parâmetros não-tipo (NTTP) e argumentos padrão de template
//  • Especialização total e parcial
//  • Templates variádicos, template de variável, template de alias
//  • `typename` e `template` em nomes dependentes
//  • `requires` e concepts (C++20)
//  • `throw` / `try` / `catch`, hierarquia de exceções e RAII
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_generico {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Template de função
// ─────────────────────────────────────────────────────────────────────────────
// `typename` e `class` são intercambiáveis aqui — pura questão de gosto.
template <typename T>
T maior(T a, T b) { return a > b ? a : b; }

template <class T>                              // idêntico ao de cima
T menor(T a, T b) { return a < b ? a : b; }

// Dois parâmetros independentes + tipo de retorno deduzido
template <typename A, typename B>
auto misturar(A a, B b) { return a + b; }

void demo_template_funcao() {
    fmt::print("\n── 8.1 Template de função ──\n");

    fmt::print("      template <typename T> T maior(T, T);\n");
    fmt::print("        maior(3, 7)          = {}     ← T deduzido como int\n", maior(3, 7));
    fmt::print("        maior(2.5, 1.5)      = {}   ← T deduzido como double\n", maior(2.5, 1.5));
    fmt::print("        maior<double>(3, 7.5)= {}   ← T dado EXPLICITAMENTE\n",
               maior<double>(3, 7.5));
    fmt::print("        maior(std::string)   = {}\n",
               maior(std::string{"abacaxi"}, std::string{"banana"}));
    fmt::print("        ⚠ maior(3, 7.5) sem o <double> NÃO compila: T não pode ser\n");
    fmt::print("          int e double ao mesmo tempo\n");

    fmt::print("\n      `typename` e `class` são sinônimos na lista de parâmetros:\n");
    fmt::print("        menor(3, 7) = {}\n", menor(3, 7));
    fmt::print("      Dois parâmetros, retorno deduzido:\n");
    fmt::print("        misturar(1, 2.5)     = {}\n", misturar(1, 2.5));
    fmt::print("        misturar(std::string{{\"a\"}}, \"b\") = {}\n",
               misturar(std::string{"a"}, "b"));

    fmt::print("\n      O compilador INSTANCIA uma função por combinação de tipos usada.\n");
    fmt::print("      Nada existe até alguém chamar — por isso o corpo mora no header.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Template de classe e CTAD
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
class Pilha {
public:
    void   empilhar(T v)  { itens_.push_back(std::move(v)); }
    T      desempilhar()  { T v = std::move(itens_.back()); itens_.pop_back(); return v; }
    bool   vazia()  const { return itens_.empty(); }
    std::size_t tamanho() const { return itens_.size(); }

    // Método que é ele próprio um template: converte para outra Pilha
    template <typename U>
    Pilha<U> convertida() const {
        Pilha<U> out;
        for (const auto& x : itens_) out.empilhar(static_cast<U>(x));
        return out;
    }

private:
    std::vector<T> itens_;
};

// Um par simples, para demonstrar CTAD
template <typename A, typename B>
struct Dupla {
    A primeiro;
    B segundo;
};
// Deduction guide (C++17): ensina o compilador a deduzir A e B do construtor
template <typename A, typename B> Dupla(A, B) -> Dupla<A, B>;

void demo_template_classe() {
    fmt::print("\n── 8.2 Template de classe e CTAD ──\n");

    Pilha<int> p;                                // o tipo é OBRIGATÓRIO aqui
    p.empilhar(1); p.empilhar(2); p.empilhar(3);

    fmt::print("      Pilha<int> p;  p.empilhar(1..3);  tamanho = {}\n", p.tamanho());

    auto pd = p.convertida<double>();
    fmt::print("      método template: p.convertida<double>() → Pilha<double>, tamanho {}\n",
               pd.tamanho());

    const int topo     = p.desempilhar();
    const int seguinte = p.desempilhar();
    fmt::print("        desempilhar() → {} depois {} (LIFO)\n", topo, seguinte);

    fmt::print("\n      CTAD (C++17) — deduzir os argumentos do próprio construtor:\n");
    Dupla d{42, std::string{"resposta"}};        // sem escrever Dupla<int, std::string>
    fmt::print("        Dupla d{{42, \"resposta\"s}};  → ({}, {})\n", d.primeiro, d.segundo);
    fmt::print("        std::vector v{{1,2,3}};      → std::vector<int>\n");
    fmt::print("        antes do C++17 era preciso `std::make_pair`, `make_tuple`...\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Parâmetros não-tipo (NTTP) e argumentos padrão
// ─────────────────────────────────────────────────────────────────────────────
// Um parâmetro de template pode ser um VALOR, não só um tipo. Ele precisa ser
// conhecido em compile-time — e é isso que faz `std::array<int, 5>` funcionar.
template <typename T, std::size_t N>
class BufferFixo {
public:
    constexpr std::size_t capacidade() const { return N; }
    T& operator[](std::size_t i) { return dados_[i]; }
private:
    std::array<T, N> dados_{};
};

// Argumentos padrão de template — como argumentos padrão de função
template <typename T = int, std::size_t N = 4>
struct Vetor { std::array<T, N> componentes{}; };

void demo_nttp() {
    fmt::print("\n── 8.3 Parâmetros não-tipo e padrões ──\n");

    BufferFixo<int, 8> b;
    b[0] = 5;

    fmt::print("      template <typename T, std::size_t N> class BufferFixo;\n");
    fmt::print("        BufferFixo<int, 8> → capacidade {} (constante de COMPILE-TIME)\n",
               b.capacidade());
    fmt::print("        b[0] = {}\n", b[0]);
    fmt::print("        BufferFixo<int,8> e BufferFixo<int,16> são tipos DIFERENTES\n");

    Vetor<> padrao;                              // = Vetor<int, 4>
    Vetor<double, 3> personalizado;
    fmt::print("\n      Argumentos padrão de template:\n");
    fmt::print("        template <typename T = int, std::size_t N = 4> struct Vetor;\n");
    fmt::print("        Vetor<>          → {} componentes de {} bytes\n",
               padrao.componentes.size(), sizeof(padrao.componentes[0]));
    fmt::print("        Vetor<double, 3> → {} componentes de {} bytes\n",
               personalizado.componentes.size(), sizeof(personalizado.componentes[0]));
    fmt::print("        ⚠ os `<>` vazios são obrigatórios — sem eles, `Vetor` é o TEMPLATE\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Especialização
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
struct NomeDoTipo { static std::string nome() { return "desconhecido"; } };

// Especialização TOTAL: `template <>` + os argumentos concretos
template <>
struct NomeDoTipo<int> { static std::string nome() { return "int"; } };

template <>
struct NomeDoTipo<std::string> { static std::string nome() { return "std::string"; } };

// Especialização PARCIAL: ainda restam parâmetros livres (só para classes)
template <typename T>
struct NomeDoTipo<T*> {
    static std::string nome() { return "ponteiro para " + NomeDoTipo<T>::nome(); }
};

template <typename T>
struct NomeDoTipo<std::vector<T>> {
    static std::string nome() { return "vector de " + NomeDoTipo<T>::nome(); }
};

void demo_especializacao() {
    fmt::print("\n── 8.4 Especialização ──\n");

    fmt::print("      genérica:        NomeDoTipo<double>  → {}\n", NomeDoTipo<double>::nome());
    fmt::print("      total (int):     NomeDoTipo<int>     → {}\n", NomeDoTipo<int>::nome());
    fmt::print("      total (string):  NomeDoTipo<std::string> → {}\n",
               NomeDoTipo<std::string>::nome());
    fmt::print("      parcial (T*):    NomeDoTipo<int*>    → {}\n", NomeDoTipo<int*>::nome());
    fmt::print("      parcial (vec):   NomeDoTipo<std::vector<int>> → {}\n",
               NomeDoTipo<std::vector<int>>::nome());
    fmt::print("      recursiva:       NomeDoTipo<std::vector<int*>> → {}\n",
               NomeDoTipo<std::vector<int*>>::nome());

    fmt::print("\n      Sintaxe:\n");
    fmt::print("        template <>            struct X<int>   {{ }};  ← total\n");
    fmt::print("        template <typename T>  struct X<T*>    {{ }};  ← parcial\n");
    fmt::print("      ⚠ FUNÇÕES não aceitam especialização parcial — use sobrecarga\n");
    fmt::print("        (ou `if constexpr`, ou concepts). Ver src/templates.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Variádicos, template de variável e nomes dependentes
// ─────────────────────────────────────────────────────────────────────────────
template <typename... Ts>
struct Lista { static constexpr std::size_t tamanho = sizeof...(Ts); };

// Template de VARIÁVEL (C++14): a base de todos os `_v` da <type_traits>
template <typename T>
constexpr bool eh_numerico_v = std::is_arithmetic_v<T>;

template <typename T>
constexpr T pi_de = static_cast<T>(3.14159265358979);

// Nome DEPENDENTE: `T::value_type` só se sabe o que é quando T for conhecido.
// Sem `typename`, o compilador assume que é um VALOR, não um tipo.
template <typename T>
typename T::value_type primeiro_elemento(const T& c) { return *c.begin(); }

void demo_variadicos_e_dependentes() {
    fmt::print("\n── 8.5 Variádicos, variable templates e nomes dependentes ──\n");

    fmt::print("      template <typename... Ts> struct Lista;\n");
    fmt::print("        Lista<int, double, char>::tamanho = {}\n",
               Lista<int, double, char>::tamanho);
    fmt::print("        Lista<>::tamanho                  = {}\n", Lista<>::tamanho);

    fmt::print("\n      Template de variável (C++14):\n");
    fmt::print("        eh_numerico_v<int>         = {}\n", eh_numerico_v<int>);
    fmt::print("        eh_numerico_v<std::string> = {}\n", eh_numerico_v<std::string>);
    fmt::print("        pi_de<float>  = {}\n", pi_de<float>);
    fmt::print("        pi_de<double> = {}\n", pi_de<double>);
    fmt::print("        → é assim que `std::is_same_v<A,B>` substituiu `::value`\n");

    std::vector<int> v{7, 8, 9};
    fmt::print("\n      Nomes dependentes — a palavra `typename` obrigatória:\n");
    fmt::print("        typename T::value_type primeiro_elemento(const T&);\n");
    fmt::print("        primeiro_elemento(vector{{7,8,9}}) = {}\n", primeiro_elemento(v));
    fmt::print("        sem `typename`, o compilador leria `T::value_type` como um VALOR\n");
    fmt::print("        (irmão disto: `obj.template metodo<T>()` em contexto dependente)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. requires e concepts (C++20)
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
concept Somavel = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;        // a expressão tem de ser válida
};

// As quatro formas de escrever a mesma restrição:
template <Somavel T>                             // (a) no lugar de `typename`
T soma_a(T a, T b) { return a + b; }

template <typename T> requires Somavel<T>        // (b) cláusula requires
T soma_b(T a, T b) { return a + b; }

template <typename T>
T soma_c(T a, T b) requires Somavel<T>           // (c) requires no fim
{ return a + b; }

auto soma_d(Somavel auto a, Somavel auto b) { return a + b; }   // (d) abreviada

void demo_concepts() {
    fmt::print("\n── 8.6 requires e concepts (C++20) ──\n");

    fmt::print("      concept Somavel = requires(T a, T b) {{ {{ a + b }} -> convertible_to<T>; }};\n");
    fmt::print("        soma_a(2, 3)                = {}   (template <Somavel T>)\n", soma_a(2, 3));
    fmt::print("        soma_b(2.5, 1.5)            = {} (requires Somavel<T>)\n", soma_b(2.5, 1.5));
    fmt::print("        soma_c(10, 20)              = {}  (requires no fim)\n", soma_c(10, 20));
    fmt::print("        soma_d(1, 2)                = {}   (sintaxe abreviada)\n", soma_d(1, 2));

    fmt::print("\n      Concepts padrão prontos para uso (<concepts>):\n");
    fmt::print("        std::integral<int>            = {}\n", std::integral<int>);
    fmt::print("        std::floating_point<int>      = {}\n", std::floating_point<int>);
    fmt::print("        std::same_as<int, int>        = {}\n", (std::same_as<int, int>));
    fmt::print("        std::convertible_to<int, double> = {}\n",
               (std::convertible_to<int, double>));

    fmt::print("\n      O ganho real é a MENSAGEM DE ERRO: em vez de 300 linhas de\n");
    fmt::print("      instanciação falhada, o compilador diz qual restrição não foi\n");
    fmt::print("      satisfeita. Cobertura completa → src/concepts.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. throw, try, catch
// ─────────────────────────────────────────────────────────────────────────────
class ErroDeDominio : public std::runtime_error {
public:
    explicit ErroDeDominio(const std::string& msg)
        : std::runtime_error("domínio: " + msg) {}
};

double dividir(double a, double b) {
    if (b == 0.0) throw std::invalid_argument("divisão por zero");
    return a / b;
}

struct Sentinela {
    std::string nome;
    explicit Sentinela(std::string n) : nome(std::move(n)) {
        fmt::print("          [{}] adquirido\n", nome);
    }
    ~Sentinela() { fmt::print("          [{}] liberado (destrutor rodou)\n", nome); }
};

void lanca_apos_adquirir() {
    Sentinela s{"recurso"};
    throw ErroDeDominio("algo deu errado depois de adquirir");
}

void demo_excecoes() {
    fmt::print("\n── 8.7 throw, try, catch ──\n");

    fmt::print("      A forma:\n");
    fmt::print("        try {{ ... }}\n");
    fmt::print("        catch (const Especifica& e) {{ ... }}   ← do mais específico\n");
    fmt::print("        catch (const std::exception& e) {{ ... }}  ← ...para o mais geral\n");
    fmt::print("        catch (...) {{ ... }}                  ← qualquer coisa\n");

    fmt::print("\n      Captura por tipo, do mais específico ao mais geral:\n");
    try {
        fmt::print("        dividir(10, 2) = {}\n", dividir(10, 2));
        fmt::print("        dividir(1, 0)  → ");
        (void)dividir(1.0, 0.0);
    } catch (const std::invalid_argument& e) {
        fmt::print("capturado invalid_argument: \"{}\"\n", e.what());
    } catch (const std::exception& e) {
        fmt::print("capturado exception: \"{}\"\n", e.what());
    }

    fmt::print("\n      Exceção própria, herdando de std::runtime_error:\n");
    try {
        lanca_apos_adquirir();
    } catch (const ErroDeDominio& e) {
        fmt::print("        capturado ErroDeDominio: \"{}\"\n", e.what());
        fmt::print("        ↑ repare: o destrutor da Sentinela rodou ANTES do catch.\n");
        fmt::print("          É o stack unwinding — a base do RAII.\n");
    }

    fmt::print("\n      catch (...) e relançar com `throw;`:\n");
    try {
        try {
            throw 42;                            // dá para lançar QUALQUER tipo...
        } catch (...) {
            fmt::print("        catch (...) pegou algo; relançando com `throw;`\n");
            throw;                               // relança a MESMA exceção
        }
    } catch (int codigo) {
        fmt::print("        o catch externo recebeu o int {} intacto\n", codigo);
    }
    fmt::print("        ...mas o combinado é lançar tipos derivados de std::exception.\n");

    fmt::print("\n      A hierarquia padrão (<stdexcept>):\n");
    fmt::print("        std::exception\n");
    fmt::print("        ├── std::logic_error    → invalid_argument, out_of_range, domain_error\n");
    fmt::print("        └── std::runtime_error  → overflow_error, range_error, system_error\n");

    fmt::print("\n      Regras práticas:\n");
    fmt::print("        • LANCE por valor, CAPTURE por `const&` (capturar por valor FATIA)\n");
    fmt::print("        • um destrutor NUNCA deve deixar escapar uma exceção\n");
    fmt::print("        • `noexcept` violado → std::terminate, sem chance de tratar\n");
    fmt::print("        • sem RAII, cada `throw` é um vazamento em potencial\n");
    fmt::print("      Garantias (básica/forte/nothrow), std::expected e error_code → src/errors.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_template_funcao();
    demo_template_classe();
    demo_nttp();
    demo_especializacao();
    demo_variadicos_e_dependentes();
    demo_concepts();
    demo_excecoes();
}

} // namespace demo_generico
