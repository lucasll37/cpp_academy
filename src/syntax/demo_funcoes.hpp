// =============================================================================
//  src/syntax/demo_funcoes.hpp
//
//  Funções — declaração, definição, sobrecarga e a sintaxe completa
//  ────────────────────────────────────────────────────────────────
//  A anatomia de uma função em C++ moderno:
//
//      [[nodiscard]] constexpr auto soma(int a, int b = 0) noexcept -> int;
//      └─ atributo ─┘└ especif. ┘└nome┘└─ parâmetros ─┘└ spec ┘└─ retorno ─┘
//
//  DECLARAR é prometer que existe (vai no .hpp);
//  DEFINIR é dizer o que faz (vai no .cpp). A ODR permite N declarações e
//  exatamente UMA definição no programa inteiro.
//
//  Tópicos:
//  • Declaração vs definição, protótipo e ODR
//  • Passagem: por valor, por referência, por const&, por ponteiro
//  • Argumentos padrão — e por que só vão na DECLARAÇÃO
//  • Sobrecarga e como o compilador resolve
//  • Retorno: `auto`, trailing return type, múltiplos valores
//  • `inline`, `noexcept`, `constexpr`, `[[nodiscard]]`, `static`
//  • Variádicas: as do C (`...`) e as de template (parameter pack)
//  • Ponteiro para função, `std::function` e lambdas como sintaxe
//  • Recursão e a versão iterativa
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdarg>
#include <functional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace demo_funcoes {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Declaração vs definição
// ─────────────────────────────────────────────────────────────────────────────

// DECLARAÇÃO (protótipo): informa nome, parâmetros e retorno. Sem corpo.
// Pode aparecer quantas vezes for preciso; os nomes dos parâmetros são opcionais.
int  multiplicar(int a, int b);
int  multiplicar(int, int);       // a mesma declaração, sem nomear os parâmetros

// DEFINIÇÃO: a declaração + o corpo. Uma única no programa (ODR).
int multiplicar(int a, int b) { return a * b; }

void demo_declaracao() {
    fmt::print("\n── 3.1 Declaração vs definição ──\n");

    fmt::print("      declaração: int multiplicar(int a, int b);      ← promessa\n");
    fmt::print("      definição:  int multiplicar(int a, int b) {{...}} ← implementação\n");
    fmt::print("      multiplicar(6, 7) = {}\n", multiplicar(6, 7));

    fmt::print("\n      Por que separar:\n");
    fmt::print("        • a declaração vai no .hpp → quem inclui sabe CHAMAR\n");
    fmt::print("        • a definição vai no .cpp  → compila uma vez, linka em todos\n");
    fmt::print("        • ODR: N declarações, exatamente UMA definição\n");
    fmt::print("        • em header-only, marque `inline` (ver demo_organizacao)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Como passar argumentos
// ─────────────────────────────────────────────────────────────────────────────
void por_valor(std::string s)              { s += " (mexido)"; }
void por_referencia(std::string& s)        { s += " (mexido)"; }
void por_const_ref(const std::string& s)   { (void)s; /* só leitura */ }
void por_ponteiro(std::string* s)          { if (s) *s += " (mexido)"; }
void por_rvalue_ref(std::string&& s)       { std::string roubado = std::move(s); (void)roubado; }

void demo_passagem() {
    fmt::print("\n── 3.2 Passagem de argumentos ──\n");

    std::string original = "texto";

    std::string a = original; por_valor(a);
    fmt::print("      por valor   `void f(std::string s)`  → \"{}\"  ← CÓPIA, original intacto\n", a);

    std::string b = original; por_referencia(b);
    fmt::print("      por ref     `void f(std::string& s)` → \"{}\"  ← modificou o original\n", b);

    std::string c = original; por_const_ref(c);
    fmt::print("      por const&  `void f(const std::string&)` → \"{}\"  ← sem cópia, sem escrita\n", c);

    std::string d = original; por_ponteiro(&d);
    fmt::print("      por ptr     `void f(std::string* s)` → \"{}\"  ← pode ser nullptr\n", d);

    std::string e = original; por_rvalue_ref(std::move(e));
    fmt::print("      por &&      `void f(std::string&& s)` → recebe temporários / move\n");

    fmt::print("\n      | forma        | copia? | modifica? | aceita nulo? | quando usar        |\n");
    fmt::print("      |--------------|--------|-----------|--------------|--------------------|\n");
    fmt::print("      | T            | sim    | não (cópia)| não         | tipos baratos, sink|\n");
    fmt::print("      | T&           | não    | SIM        | não          | saída, in-out      |\n");
    fmt::print("      | const T&     | não    | não        | não          | leitura (padrão)   |\n");
    fmt::print("      | T*           | não    | sim        | SIM          | opcional/C API     |\n");
    fmt::print("      | T&&          | não    | consome    | não          | move, forwarding   |\n");
    fmt::print("      Regra prática: `const T&` para ler, `T` para tipos pequenos\n");
    fmt::print("      (int, double, ponteiro), `T&` só quando a saída é o objetivo.\n");
    fmt::print("      Detalhes de move e forwarding → src/value_semantics.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Argumentos padrão
// ─────────────────────────────────────────────────────────────────────────────
// Os padrões vão na DECLARAÇÃO (aqui, header-only, é a mesma linha).
// Regra: uma vez que um parâmetro tem padrão, todos à direita também precisam ter.
std::string formatar(const std::string& texto,
                     char  preenchimento = '.',
                     int   largura       = 20) {
    std::string out = texto;
    while (out.size() < static_cast<std::size_t>(largura)) out += preenchimento;
    return out;
}

void demo_default_args() {
    fmt::print("\n── 3.3 Argumentos padrão ──\n");

    fmt::print("      formatar(\"abc\")            → [{}]\n", formatar("abc"));
    fmt::print("      formatar(\"abc\", '-')       → [{}]\n", formatar("abc", '-'));
    fmt::print("      formatar(\"abc\", '-', 10)   → [{}]\n", formatar("abc", '-', 10));

    fmt::print("\n      Regras:\n");
    fmt::print("        • só se preenche da direita para a esquerda (não há named args)\n");
    fmt::print("        • depois do primeiro padrão, TODOS à direita precisam ter um\n");
    fmt::print("        • o padrão pertence à DECLARAÇÃO — repeti-lo na definição é erro\n");
    fmt::print("        • ⚠ o padrão é resolvido no CHAMADOR: numa função virtual, ele\n");
    fmt::print("          vem do tipo ESTÁTICO, não do dinâmico. Não use padrão em virtual.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Sobrecarga (overloading)
// ─────────────────────────────────────────────────────────────────────────────
std::string tipo_de(int)                { return "int"; }
std::string tipo_de(double)             { return "double"; }
std::string tipo_de(const char*)        { return "const char*"; }
std::string tipo_de(const std::string&) { return "const std::string&"; }
std::string tipo_de(bool)               { return "bool"; }
std::string tipo_de(int, int)           { return "int, int (aridade diferente)"; }

void demo_sobrecarga() {
    fmt::print("\n── 3.4 Sobrecarga ──\n");

    fmt::print("      tipo_de(42)         → {}\n", tipo_de(42));
    fmt::print("      tipo_de(3.14)       → {}\n", tipo_de(3.14));
    fmt::print("      tipo_de(\"literal\")  → {}\n", tipo_de("literal"));
    fmt::print("      tipo_de(std::string) → {}\n", tipo_de(std::string{"s"}));
    fmt::print("      tipo_de(true)       → {}\n", tipo_de(true));
    fmt::print("      tipo_de(1, 2)       → {}\n", tipo_de(1, 2));

    fmt::print("\n      A assinatura é (nome, tipos dos parâmetros, const/ref da função).\n");
    fmt::print("      ⚠ O TIPO DE RETORNO **não** entra: estas duas colidem —\n");
    fmt::print("          int  f(int);\n");
    fmt::print("          bool f(int);   ← erro: redeclaração com retorno diferente\n");

    fmt::print("\n      Ordem da resolução (o compilador tenta nesta sequência):\n");
    fmt::print("        1. correspondência exata\n");
    fmt::print("        2. promoção      (char→int, float→double)\n");
    fmt::print("        3. conversão padrão (int→double, T*→void*, →bool)\n");
    fmt::print("        4. conversão definida pelo usuário (construtor, operator T)\n");
    fmt::print("        5. reticências (...)\n");
    fmt::print("      Empate entre duas candidatas do mesmo nível → ambiguidade (erro).\n");
    fmt::print("      Sobrecarga de OPERADORES → src/operators.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O tipo de retorno
// ─────────────────────────────────────────────────────────────────────────────

// Trailing return type: indispensável quando o retorno depende dos parâmetros
template <typename A, typename B>
auto somar(A a, B b) -> decltype(a + b) { return a + b; }

// Retorno deduzido (C++14): todos os `return` precisam concordar no tipo
auto metade(int x) { return x / 2.0; }   // → double

// Vários valores de uma vez, sem parâmetros de saída
std::pair<int, int> divmod(int a, int b) { return {a / b, a % b}; }

struct Estatisticas { int minimo; int maximo; double media; };
Estatisticas analisar(const std::vector<int>& v) {
    int mn = v.front(), mx = v.front(), soma = 0;
    for (int x : v) { mn = x < mn ? x : mn; mx = x > mx ? x : mx; soma += x; }
    return {mn, mx, static_cast<double>(soma) / static_cast<double>(v.size())};
}

void demo_retorno() {
    fmt::print("\n── 3.5 O tipo de retorno ──\n");

    fmt::print("      trailing return  `auto f(A,B) -> decltype(a+b)`:\n");
    fmt::print("        somar(1, 2)     → {}   (int)\n",    somar(1, 2));
    fmt::print("        somar(1, 2.5)   → {}  (double)\n",  somar(1, 2.5));

    fmt::print("\n      retorno deduzido `auto metade(int)`:\n");
    fmt::print("        metade(7) → {}   ← deduziu double por causa do 2.0\n", metade(7));

    fmt::print("\n      Vários valores — sem parâmetros de saída:\n");
    auto [q, r] = divmod(17, 5);
    fmt::print("        std::pair  : divmod(17,5) → quociente {} resto {}\n", q, r);
    auto [mn, mx, media] = analisar({4, 8, 15, 16, 23, 42});
    fmt::print("        struct     : analisar(...) → min {} max {} média {:.2f}\n",
               mn, mx, media);
    fmt::print("        → prefira uma struct nomeada a `std::tuple`: os campos têm NOME.\n");

    fmt::print("\n      ⚠ NUNCA devolva referência ou ponteiro para variável local:\n");
    fmt::print("          int& f() {{ int x = 1; return x; }}  ← dangling, UB\n");
    fmt::print("        Devolver por valor é barato: RVO/NRVO eliminam a cópia.\n");
    fmt::print("        Ver src/value_semantics.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Especificadores: inline, constexpr, noexcept, [[nodiscard]], static
// ─────────────────────────────────────────────────────────────────────────────

// `inline` em C++ moderno significa "várias definições idênticas são OK",
// não "insira o corpo aqui" (essa decisão é do otimizador).
inline int dobro(int x) { return 2 * x; }

// `constexpr` → pode ser avaliada em compile-time (se os argumentos permitirem)
constexpr int fatorial(int n) { return n <= 1 ? 1 : n * fatorial(n - 1); }

// `noexcept` → promete não lançar. Se lançar mesmo assim: std::terminate.
int sempre_seguro(int x) noexcept { return x + 1; }

// `[[nodiscard]]` → o compilador AVISA se o retorno for ignorado
[[nodiscard]] bool validar(int x) { return x > 0; }

// `static` em escopo de arquivo → linkage interna: invisível a outras .cpp
static int ajudante_privado(int x) { return x - 1; }

// `[[maybe_unused]]`, `[[deprecated]]`
[[deprecated("use validar() em vez desta")]]
bool checar_antigo(int x) { return x > 0; }

void demo_especificadores() {
    fmt::print("\n── 3.6 Especificadores de função ──\n");

    constexpr int f5 = fatorial(5);          // calculado em COMPILE-TIME
    static_assert(f5 == 120, "avaliado pelo compilador");

    fmt::print("      inline int dobro(int)         → dobro(21) = {}\n", dobro(21));
    fmt::print("        significa \"pode ter definição repetida\", não \"expanda o corpo\"\n");
    fmt::print("      constexpr int fatorial(int)   → fatorial(5) = {} (em compile-time)\n", f5);
    fmt::print("      int f(int) noexcept           → sempre_seguro(1) = {}\n", sempre_seguro(1));
    fmt::print("        promessa: se lançar, o programa CHAMA std::terminate\n");
    fmt::print("      [[nodiscard]] bool validar()  → ignorar o retorno gera warning\n");
    fmt::print("        validar(5) = {}\n", validar(5));
    fmt::print("      static int ajudante(int)      → só visível nesta .cpp = {}\n",
               ajudante_privado(10));
    fmt::print("      [[deprecated(\"...\")]]         → warning em cada uso\n");
    fmt::print("      [[maybe_unused]]              → silencia o warning de não-uso\n");

    fmt::print("\n      Detalhes de `noexcept` e garantias → src/errors.\n");
    fmt::print("      Detalhes de `constexpr`/`consteval` → src/compile_time.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Funções variádicas
// ─────────────────────────────────────────────────────────────────────────────

// (a) A forma do C: <cstdarg>. Sem segurança de tipos — o chamador tem de dizer
//     quantos argumentos vêm, e um erro só aparece em runtime.
int soma_estilo_c(int quantos, ...) {
    std::va_list args;
    va_start(args, quantos);
    int total = 0;
    for (int i = 0; i < quantos; ++i) total += va_arg(args, int);
    va_end(args);
    return total;
}

// (b) A forma do C++: parameter pack. Com tipos, sem contagem manual,
//     e resolvido em compile-time.
template <typename... Args>
auto soma_variadica(Args... args) { return (args + ...); }   // fold expression (C++17)

template <typename... Args>
void imprimir_todos(const Args&... args) {
    ((fmt::print("[{}] ", args)), ...);   // fold sobre o operador vírgula
    fmt::print("({} argumentos)\n", sizeof...(args));
}

void demo_variadicas() {
    fmt::print("\n── 3.7 Funções variádicas ──\n");

    fmt::print("      estilo C   `int f(int n, ...)` + <cstdarg>:\n");
    fmt::print("        soma_estilo_c(3, 10, 20, 30) = {}\n", soma_estilo_c(3, 10, 20, 30));
    fmt::print("        ⚠ sem tipos, sem contagem automática — errar a contagem é UB\n");

    fmt::print("\n      estilo C++ `template <typename... Args>` + fold expression:\n");
    fmt::print("        soma_variadica(1, 2, 3, 4)     = {}\n", soma_variadica(1, 2, 3, 4));
    fmt::print("        soma_variadica(1.5, 2.5)       = {}\n", soma_variadica(1.5, 2.5));
    fmt::print("        imprimir_todos(1, \"dois\", 3.0) → ");
    imprimir_todos(1, "dois", 3.0);
    fmt::print("        `sizeof...(args)` conta o pack em compile-time\n");
    fmt::print("      Detalhes de packs e folds → src/templates.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  8. Funções como VALORES: ponteiro, std::function, lambda
// ─────────────────────────────────────────────────────────────────────────────
int aplicar(int a, int b, int (*op)(int, int)) { return op(a, b); }

int somar_int(int a, int b)    { return a + b; }
int subtrair_int(int a, int b) { return a - b; }

void demo_funcoes_como_valores() {
    fmt::print("\n── 3.8 Funções como valores ──\n");

    // (a) Ponteiro para função — a forma do C, zero overhead, zero estado
    int (*op)(int, int) = &somar_int;      // o `&` é opcional
    fmt::print("      ponteiro:  int (*op)(int,int) = somar_int;\n");
    fmt::print("        op(10, 3)              = {}\n", op(10, 3));
    op = subtrair_int;
    fmt::print("        após op = subtrair_int → {}\n", op(10, 3));
    fmt::print("        aplicar(10, 3, somar_int) = {}\n", aplicar(10, 3, somar_int));

    // (b) Lambda — uma classe anônima com operator(), escrita inline
    auto multiplicar_l = [](int a, int b) { return a * b; };
    fmt::print("\n      lambda:    auto f = [](int a, int b) {{ return a * b; }};\n");
    fmt::print("        f(10, 3) = {}\n", multiplicar_l(10, 3));

    // Lambda SEM captura converte-se implicitamente em ponteiro de função
    int (*como_ptr)(int, int) = multiplicar_l;
    fmt::print("        lambda sem captura → converte para ponteiro: {}\n",
               como_ptr(10, 3));

    // (c) std::function — guarda QUALQUER chamável com essa assinatura
    std::function<int(int, int)> qualquer = somar_int;
    fmt::print("\n      std::function<int(int,int)> — aceita qualquer chamável:\n");
    fmt::print("        = somar_int            → {}\n", qualquer(10, 3));
    qualquer = multiplicar_l;
    fmt::print("        = lambda               → {}\n", qualquer(10, 3));
    int base = 100;
    qualquer = [base](int a, int b) { return base + a + b; };
    fmt::print("        = lambda COM captura   → {}  ← o ponteiro cru não conseguiria\n",
               qualquer(10, 3));

    fmt::print("\n      Capturas, type erasure e custo → src/lambdas.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  9. Recursão
// ─────────────────────────────────────────────────────────────────────────────
unsigned long long fib_recursivo(int n) {
    if (n < 2) return static_cast<unsigned long long>(n);        // caso base
    return fib_recursivo(n - 1) + fib_recursivo(n - 2);          // caso recursivo
}

unsigned long long fib_iterativo(int n) {
    unsigned long long a = 0, b = 1;
    for (int i = 0; i < n; ++i) { auto t = a + b; a = b; b = t; }
    return a;
}

// Recursão mútua: cada uma precisa da declaração da outra
bool eh_impar(int n);
bool eh_par(int n)   { return n == 0 ? true  : eh_impar(n - 1); }
bool eh_impar(int n) { return n == 0 ? false : eh_par(n - 1); }

void demo_recursao() {
    fmt::print("\n── 3.9 Recursão ──\n");

    fmt::print("      fib_recursivo(20) = {}   ← O(2^n): recalcula tudo\n", fib_recursivo(20));
    fmt::print("      fib_iterativo(20) = {}   ← O(n)\n", fib_iterativo(20));
    fmt::print("        toda recursão precisa de um CASO BASE — sem ele, stack overflow\n");

    fmt::print("\n      Recursão mútua (exige declaração antecipada):\n");
    fmt::print("        eh_par(10) = {}   eh_impar(7) = {}\n", eh_par(10), eh_impar(7));

    fmt::print("\n      A pilha é finita (~8 MB no Linux): recursão profunda estoura.\n");
    fmt::print("      O compilador PODE aplicar tail-call optimization, mas não é garantido.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_declaracao();
    demo_passagem();
    demo_default_args();
    demo_sobrecarga();
    demo_retorno();
    demo_especificadores();
    demo_variadicas();
    demo_funcoes_como_valores();
    demo_recursao();
}

} // namespace demo_funcoes
