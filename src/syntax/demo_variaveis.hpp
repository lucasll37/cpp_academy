// =============================================================================
//  src/syntax/demo_variaveis.hpp
//
//  Declarações — tipos, variáveis, inicialização e tempo de vida
//  ─────────────────────────────────────────────────────────────
//  Toda declaração em C++ tem a mesma forma:
//
//      [especificadores]  [declarador]  [inicializador] ;
//         static const       int* p          = &x
//         ↑ storage/cv       ↑ tipo+nome     ↑ como nasce
//
//  Ler uma declaração é seguir a "regra da espiral": comece no nome, vá para a
//  direita enquanto puder, depois para a esquerda.
//
//      int* p[5];      // p é array[5] de ponteiro para int
//      int (*q)[5];    // q é ponteiro para array[5] de int
//      int (*f)(int);  // f é ponteiro para função(int) que devolve int
//
//  Tópicos:
//  • Tipos fundamentais e seus tamanhos reais
//  • As 6 formas de inicializar — e por que `{}` é a mais segura
//  • `auto`, `decltype` e `decltype(auto)`
//  • `const`, `constexpr`, `constinit`, `volatile`, `mutable`
//  • Escopo, tempo de vida e storage class (`static`, `thread_local`, `extern`)
//  • Ponteiros, referências e arrays — e o decay
//  • `typedef` vs `using`
//  • Literais: sufixos, bases, separador de dígitos, raw strings
//  • Structured bindings (C++17)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace demo_variaveis {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Tipos fundamentais — o que a linguagem garante e o que não garante
// ─────────────────────────────────────────────────────────────────────────────
void demo_tipos_fundamentais() {
    fmt::print("\n── 1.1 Tipos fundamentais ──\n");

    // O padrão só garante: 1 == sizeof(char) <= sizeof(short) <= sizeof(int)
    //                        <= sizeof(long) <= sizeof(long long)
    fmt::print("      bool                = {} byte\n",  sizeof(bool));
    fmt::print("      char                = {} byte   (assinado ou não: implementação)\n",
               sizeof(char));
    fmt::print("      signed char         = {} byte\n",  sizeof(signed char));
    fmt::print("      short               = {} bytes\n", sizeof(short));
    fmt::print("      int                 = {} bytes\n", sizeof(int));
    fmt::print("      long                = {} bytes  (4 no Windows, 8 no Linux!)\n",
               sizeof(long));
    fmt::print("      long long           = {} bytes\n", sizeof(long long));
    fmt::print("      float               = {} bytes\n", sizeof(float));
    fmt::print("      double              = {} bytes\n", sizeof(double));
    fmt::print("      long double         = {} bytes\n", sizeof(long double));
    fmt::print("      void*               = {} bytes\n", sizeof(void*));

    // Quando o TAMANHO importa (protocolos, arquivos binários, registradores),
    // use os tipos de largura fixa de <cstdint>.
    std::uint8_t  byte_sem_sinal = 255;
    std::int32_t  inteiro_32     = -2'000'000'000;
    std::size_t   tamanho        = sizeof(std::size_t);
    std::ptrdiff_t distancia     = 0;

    fmt::print("\n      Largura fixa (<cstdint>) — use em protocolos e I/O binário:\n");
    fmt::print("        uint8_t  = {}  → valor {}\n", sizeof(byte_sem_sinal), byte_sem_sinal);
    fmt::print("        int32_t  = {}  → valor {}\n", sizeof(inteiro_32), inteiro_32);
    fmt::print("        size_t   = {}  → índices e tamanhos (SEM sinal!)\n", tamanho);
    fmt::print("        ptrdiff_t= {}  → diferença entre ponteiros (COM sinal)\n",
               sizeof(distancia));

    // std::byte (C++17): "isto é memória crua", não um número nem um caractere
    std::byte b{0b1010'1010};
    b = b >> 4;
    fmt::print("        std::byte= {}  → só operações de bit, sem aritmética\n", sizeof(b));
    fmt::print("        0b10101010 >> 4 = {}\n", std::to_integer<int>(b));

    fmt::print("\n      ⚠ `char`, `signed char` e `unsigned char` são TRÊS tipos distintos.\n");
    fmt::print("        char é o mesmo tipo que signed char? {}\n",
               std::is_same_v<char, signed char>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. As formas de inicializar
// ─────────────────────────────────────────────────────────────────────────────
void demo_inicializacao() {
    fmt::print("\n── 1.2 As formas de inicializar ──\n");

    int copia      = 42;      // copy-initialization
    int direta(42);           // direct-initialization
    int uniforme{42};         // list-initialization  ← preferida
    int copia_lista = {42};   // copy-list-initialization
    int valor{};              // value-initialization → ZERO garantido
    // int indeterminada;     // default-init de tipo trivial → LIXO. Não leia!

    fmt::print("      int x = 42;    → {}   copy-init\n",   copia);
    fmt::print("      int x(42);     → {}   direct-init\n", direta);
    fmt::print("      int x{{42}};     → {}   list-init (preferida)\n", uniforme);
    fmt::print("      int x = {{42}};  → {}   copy-list-init\n", copia_lista);
    fmt::print("      int x{{}};       → {}    value-init → SEMPRE zero\n", valor);
    fmt::print("      int x;         → ???  default-init → indeterminado (UB ao ler)\n");

    // Por que `{}` é a forma preferida: ela PROÍBE conversões que perdem dados
    double  pi = 3.14159;
    int     truncado = pi;    // permitido, silencioso — perde a parte fracionária
    // int  seguro{pi};       // ✗ ERRO de compilação: narrowing conversion

    fmt::print("\n      Narrowing (perda silenciosa de dados):\n");
    fmt::print("        int t = 3.14159;  → {}      ← compila, perde dados\n", truncado);
    fmt::print("        int s{{3.14159}};   → ERRO    ← as chaves BLOQUEIAM\n");

    // ⚠ A exceção que dói: com std::vector e initializer_list, `{}` muda o SIGNIFICADO
    fmt::print("\n      ⚠ A exceção: containers com initializer_list\n");
    fmt::print("        std::vector<int> a(3, 0);  → 3 elementos iguais a 0\n");
    fmt::print("        std::vector<int> b{{3, 0}};  → 2 elementos: 3 e 0\n");

    // ⚠ Most vexing parse: o que parece variável e é declaração de função
    fmt::print("\n      ⚠ Most vexing parse:\n");
    fmt::print("        Widget w();     ← NÃO é variável: é declaração de FUNÇÃO\n");
    fmt::print("        Widget w{{}};     ← isto sim é a variável\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. auto, decltype e decltype(auto)
// ─────────────────────────────────────────────────────────────────────────────
void demo_auto() {
    fmt::print("\n── 1.3 auto e decltype ──\n");

    auto        i = 42;           // int
    auto        d = 42.0;         // double
    auto        c = 'x';          // char
    auto        s = "texto";      // const char*  ← NÃO é std::string!
    const auto  k = 42;           // const int
    auto*       p = &i;           // int*
    auto&       r = i;            // int&
    const auto& cr = i;           // const int&
    auto&&      fw = i;           // int&  (forwarding reference colapsa)

    fmt::print("      auto i = 42;        → int      ({} bytes)\n", sizeof(i));
    fmt::print("      auto d = 42.0;      → double   ({} bytes)\n", sizeof(d));
    fmt::print("      auto c = 'x';       → char     ({} byte)\n",  sizeof(c));
    fmt::print("      auto s = \"texto\";   → const char* — NÃO é std::string\n");
    fmt::print("      auto* p = &i;       → int*\n");
    fmt::print("      auto& r = i;        → int&      (r e i são o mesmo objeto)\n");

    // ⚠ `auto` puro DESCARTA referência, const e volatile
    const int&  origem = i;
    auto        copia  = origem;   // int — perdeu const E referência
    copia = 999;                   // legal: é uma cópia independente

    fmt::print("\n      ⚠ `auto` puro descarta &, const e volatile:\n");
    fmt::print("        const int& origem = i;   auto copia = origem;\n");
    fmt::print("        decltype(copia) é int?      {}\n", std::is_same_v<decltype(copia), int>);
    fmt::print("        i continua {} — a cópia virou {}\n", i, copia);
    fmt::print("        → para preservar tudo, escreva `const auto&` ou `decltype(auto)`\n");

    // decltype preserva EXATAMENTE o tipo declarado
    fmt::print("\n      decltype preserva o tipo exato:\n");
    fmt::print("        decltype(origem) é const int&?  {}\n",
               std::is_same_v<decltype(origem), const int&>);
    fmt::print("        decltype(r)      é int&?        {}\n",
               std::is_same_v<decltype(r), int&>);
    fmt::print("        decltype(i + d)  é double?      {}\n",
               std::is_same_v<decltype(i + d), double>);

    // decltype((x)) — os parênteses extras mudam o resultado para lvalue reference
    fmt::print("        decltype(i)   é int?            {}\n",
               std::is_same_v<decltype(i), int>);
    fmt::print("        decltype((i)) é int&?           {}  ← parênteses mudam tudo!\n",
               std::is_same_v<decltype((i)), int&>);

    (void)k; (void)cr; (void)fw; (void)p; (void)s;
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. const, constexpr, constinit, volatile, mutable
// ─────────────────────────────────────────────────────────────────────────────

// `constinit` (C++20): inicialização garantida em compile-time, mas MUTÁVEL depois.
// Resolve o "static initialization order fiasco" sem forçar constância.
constinit int contador_global = 0;

void demo_qualificadores() {
    fmt::print("\n── 1.4 const, constexpr, constinit ──\n");

    const int    runtime_const = 42;   // não muda; valor pode vir de runtime
    constexpr int compile_const = 42;  // conhecido em COMPILE-TIME
    static_assert(compile_const == 42, "constexpr é utilizável em static_assert");

    fmt::print("      const     → \"não posso modificar\"   (pode vir de runtime)\n");
    fmt::print("      constexpr → \"o compilador já sabe\"  (usável em static_assert, NTTP)\n");
    fmt::print("      constinit → inicializa em compile-time, mas PODE mudar depois\n");

    contador_global++;
    fmt::print("        constinit int contador = 0;  após ++ → {}\n", contador_global);

    // Ponteiros e const: a regra é ler da DIREITA para a ESQUERDA
    int a = 1, b = 2;
    const int*       ptr_para_const = &a;   // "ponteiro para int const"
    int* const       const_ptr      = &a;   // "ponteiro const para int"
    const int* const tudo_const     = &a;   // ambos

    ptr_para_const = &b;    // ✓ o ponteiro pode apontar para outro
    // *ptr_para_const = 9; // ✗ mas não pode escrever através dele
    *const_ptr = 10;        // ✓ pode escrever
    // const_ptr = &b;      // ✗ mas não pode reapontar

    fmt::print("\n      const e ponteiros — leia da DIREITA para a ESQUERDA:\n");
    fmt::print("        const int* p;        p aponta para const  → *p = x ✗ | p = &y ✓\n");
    fmt::print("        int* const p;        p é const            → *p = x ✓ | p = &y ✗\n");
    fmt::print("        const int* const p;  ambos                → *p = x ✗ | p = &y ✗\n");
    fmt::print("        *ptr_para_const lê {} (mas não escreve); a agora vale {}\n",
               *ptr_para_const, a);

    // `volatile` NÃO tem nada a ver com threads — é sobre I/O mapeado em memória
    volatile int registrador_hw = 0;
    registrador_hw = 1;
    fmt::print("\n      volatile → \"não otimize esta leitura/escrita\" (hardware, signal)\n");
    fmt::print("        ⚠ volatile NÃO dá atomicidade nem ordenação — use std::atomic\n");

    (void)runtime_const; (void)tudo_const; (void)registrador_hw;
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Escopo, tempo de vida e storage class
// ─────────────────────────────────────────────────────────────────────────────
int contador_de_chamadas() {
    // `static` local: inicializado UMA vez, na primeira passagem; vive até o fim
    // do programa. Desde o C++11 a inicialização é thread-safe ("magic static").
    static int chamadas = 0;
    return ++chamadas;
}

void demo_escopo() {
    fmt::print("\n── 1.5 Escopo, tempo de vida e storage class ──\n");

    int x = 1;                      // escopo da função
    fmt::print("      escopo externo: x = {}\n", x);
    {
        int x = 2;                  // NOVA variável — sombreia a de fora
        fmt::print("        bloco interno: x = {}  ← sombreamento (shadowing)\n", x);
        {
            int y = 3;
            fmt::print("          bloco mais interno: y = {}\n", y);
        }
        // `y` já não existe aqui: destruído no fim do bloco
    }
    fmt::print("      de volta ao externo: x = {}  ← o de dentro foi destruído\n", x);

    fmt::print("\n      static local — inicializa uma vez só:\n");
    fmt::print("        chamada 1 → {}\n", contador_de_chamadas());
    fmt::print("        chamada 2 → {}\n", contador_de_chamadas());
    fmt::print("        chamada 3 → {}\n", contador_de_chamadas());

    // thread_local: uma instância POR THREAD
    thread_local int por_thread = 0;
    por_thread += 10;
    fmt::print("\n      thread_local → uma cópia por thread (aqui: {})\n", por_thread);

    fmt::print("\n      | storage        | vive quando          | visível onde        |\n");
    fmt::print("      |----------------|----------------------|---------------------|\n");
    fmt::print("      | (automático)   | escopo do bloco      | bloco               |\n");
    fmt::print("      | static (local) | programa inteiro     | bloco               |\n");
    fmt::print("      | static (global)| programa inteiro     | só esta .cpp        |\n");
    fmt::print("      | extern         | programa inteiro     | todo o programa     |\n");
    fmt::print("      | thread_local   | vida da thread       | conforme declarado  |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Ponteiros, referências e arrays
// ─────────────────────────────────────────────────────────────────────────────
void recebe_array_decaido(const int* p, std::size_t n) {
    fmt::print("        dentro da função: sizeof(p) = {} ← virou PONTEIRO, "
               "n veio separado ({})\n", sizeof(p), n);
}

// Referência para array: preserva o tamanho no tipo
void recebe_array_por_ref(const int (&arr)[5]) {
    fmt::print("        por referência: sizeof(arr) = {} ← o tamanho SOBREVIVEU\n",
               sizeof(arr));
}

void demo_ponteiros_refs() {
    fmt::print("\n── 1.6 Ponteiros, referências e arrays ──\n");

    int  valor = 42;
    int* ptr   = &valor;    // ponteiro: guarda um endereço, pode ser nulo, reapontável
    int& ref   = valor;     // referência: outro NOME para o mesmo objeto

    *ptr = 100;
    fmt::print("      int valor = 42;  int* ptr = &valor;  *ptr = 100;\n");
    fmt::print("        valor agora = {}   ref = {}   (todos o MESMO objeto)\n", valor, ref);

    ref = 200;
    fmt::print("      ref = 200;  →  valor = {}   *ptr = {}\n", valor, *ptr);

    int* nulo = nullptr;    // nullptr (C++11) — não use NULL nem 0
    fmt::print("\n      | aspecto        | ponteiro       | referência        |\n");
    fmt::print("      |----------------|----------------|-------------------|\n");
    fmt::print("      | pode ser nulo  | sim (nullptr)  | não               |\n");
    fmt::print("      | reapontável    | sim            | não (liga uma vez)|\n");
    fmt::print("      | precisa de *   | sim            | não               |\n");
    fmt::print("      | aritmética     | sim            | não               |\n");
    fmt::print("        ptr == nullptr? {}\n", nulo == nullptr);
    fmt::print("        ⚠ desreferenciar nullptr é UB — verifique antes\n");

    // Arrays em C: o tipo carrega o tamanho... até virar argumento de função
    int arr[5] = {1, 2, 3, 4, 5};
    fmt::print("\n      Array e decay:\n");
    fmt::print("        int arr[5];  sizeof(arr) = {} = 5 * sizeof(int)\n", sizeof(arr));
    fmt::print("        arr[0]={} arr[4]={}  — sem verificação de limites!\n",
               arr[0], arr[4]);
    recebe_array_decaido(arr, std::size(arr));
    recebe_array_por_ref(arr);

    // Aritmética de ponteiro: arr[i] é literalmente *(arr + i)
    fmt::print("\n      arr[2] == *(arr + 2) == 2[arr]?  {} {} {}\n",
               arr[2], *(arr + 2), 2[arr]);

    // std::array: o mesmo array, mas com tipo, tamanho e iteradores decentes
    std::array<int, 5> moderno{1, 2, 3, 4, 5};
    fmt::print("\n      std::array<int,5>: sizeof = {} (idêntico), mas com .size()={}, \n",
               sizeof(moderno), moderno.size());
    fmt::print("        .at() com verificação, e SEM decay ao passar por valor.\n");

    (void)nulo;
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. typedef vs using
// ─────────────────────────────────────────────────────────────────────────────
typedef unsigned long   Antigo;          // sintaxe herdada do C
using                   Moderno = unsigned long;   // C++11 — lê-se da esquerda

typedef int (*CallbackAntigo)(int, int);           // ilegível
using   Callback = int (*)(int, int);              // legível

template <typename T>
using Par = std::pair<T, T>;             // alias TEMPLATE — typedef não consegue isto

void demo_alias() {
    fmt::print("\n── 1.7 typedef vs using ──\n");

    Antigo  a = 1;
    Moderno m = 2;
    Par<int> p{10, 20};

    fmt::print("      typedef unsigned long Antigo;      → a = {}\n", a);
    fmt::print("      using Moderno = unsigned long;     → m = {}\n", m);
    fmt::print("      são o mesmo tipo? {}\n", std::is_same_v<Antigo, Moderno>);

    fmt::print("\n      A diferença que decide: ponteiro para função\n");
    fmt::print("        typedef int (*CB)(int,int);   ← o nome fica NO MEIO\n");
    fmt::print("        using   CB = int (*)(int,int);← nome à esquerda, tipo à direita\n");

    fmt::print("\n      E o que só `using` faz — alias template:\n");
    fmt::print("        template <typename T> using Par = std::pair<T,T>;\n");
    fmt::print("        Par<int> p{{10,20}} → ({}, {})\n", p.first, p.second);
    fmt::print("        `typedef` NÃO pode ser templatizado.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  8. Literais
// ─────────────────────────────────────────────────────────────────────────────
void demo_literais() {
    fmt::print("\n── 1.8 Literais ──\n");

    // Bases
    int decimal     = 42;
    int octal       = 052;          // prefixo 0    — cuidado com zeros à esquerda!
    int hexadecimal = 0x2A;         // prefixo 0x
    int binario     = 0b101010;     // prefixo 0b   (C++14)
    int legivel     = 1'000'000;    // separador '  (C++14)

    fmt::print("      42 == 052 == 0x2A == 0b101010 ? {} {} {} {}\n",
               decimal, octal, hexadecimal, binario);
    fmt::print("      1'000'000 → {}   ← o apóstrofo é só visual\n", legivel);
    fmt::print("      ⚠ 052 é OCTAL (=42), não 52. Zero à esquerda muda a base.\n");

    // Sufixos — mudam o TIPO do literal
    auto u  = 42u;      // unsigned int
    auto l  = 42L;      // long
    auto ll = 42LL;     // long long
    auto ul = 42UL;     // unsigned long
    auto f  = 3.14f;    // float
    auto d  = 3.14;     // double     (padrão)
    auto ld = 3.14L;    // long double

    fmt::print("\n      Sufixos definem o TIPO:\n");
    fmt::print("        42u  → unsigned ({}B) | 42L  → long ({}B) | 42LL → long long ({}B)\n",
               sizeof(u), sizeof(l), sizeof(ll));
    fmt::print("        3.14f→ float ({}B)    | 3.14 → double ({}B) | 3.14L→ long double ({}B)\n",
               sizeof(f), sizeof(d), sizeof(ld));

    // Caracteres e strings
    char        ch    = 'A';
    const char* texto = "linha 1\nlinha 2";
    const char* raw   = R"(C:\pasta\sem\escapes  "aspas"  \n literal)";

    fmt::print("\n      'A' = {} (código {})\n", ch, static_cast<int>(ch));
    fmt::print("      \"com \\n\"  → escapes interpretados\n");
    fmt::print("      R\"(...)\"   → raw string, nada é escapado:\n        {}\n", raw);

    // Literais definidos pelo usuário: ver src/operators
    using namespace std::string_literals;
    auto s = "sou std::string"s;   // sufixo `s` do <string>
    fmt::print("\n      \"texto\"s → std::string (literal de usuário) → \"{}\" ({} chars)\n",
               s, s.size());
    fmt::print("        UDLs próprios (`operator\"\"_km`) → ver src/operators\n");

    (void)texto; (void)ul;
}

// ─────────────────────────────────────────────────────────────────────────────
//  9. Structured bindings (C++17)
// ─────────────────────────────────────────────────────────────────────────────
struct Ponto3D { double x, y, z; };

void demo_structured_bindings() {
    fmt::print("\n── 1.9 Structured bindings (C++17) ──\n");

    std::pair<int, std::string> par{1, "um"};
    auto [numero, nome] = par;
    fmt::print("      de std::pair:   [numero, nome] = [{}, {}]\n", numero, nome);

    std::tuple<int, double, char> tupla{7, 2.5, 'z'};
    auto [i, d, c] = tupla;
    fmt::print("      de std::tuple:  [i, d, c] = [{}, {}, {}]\n", i, d, c);

    Ponto3D p{1.0, 2.0, 3.0};
    auto [x, y, z] = p;
    fmt::print("      de struct:      [x, y, z] = [{}, {}, {}]\n", x, y, z);

    // Com referência: modifica o original
    auto& [rx, ry, rz] = p;
    rx = 99.0;
    fmt::print("      auto& [rx,...] = p;  rx = 99  →  p.x agora é {}\n", p.x);

    int arr[3] = {10, 20, 30};
    auto [a0, a1, a2] = arr;
    fmt::print("      de array C:     [{}, {}, {}]\n", a0, a1, a2);

    fmt::print("\n      Regras: o número de nomes tem de bater EXATAMENTE, e os\n");
    fmt::print("      membros precisam ser todos públicos (para structs).\n");

    (void)rz; (void)ry;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_tipos_fundamentais();
    demo_inicializacao();
    demo_auto();
    demo_qualificadores();
    demo_escopo();
    demo_ponteiros_refs();
    demo_alias();
    demo_literais();
    demo_structured_bindings();
}

} // namespace demo_variaveis
