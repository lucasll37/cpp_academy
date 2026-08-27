// =============================================================================
//  src/lambdas/demo_capturas.hpp
//
//  Capturas — o que a lambda leva consigo
//  ──────────────────────────────────────
//  Uma lambda não é uma construção mágica: o compilador gera uma CLASSE anônima
//  com `operator()`, e as capturas viram MEMBROS dessa classe.
//
//      auto f = [x](int y) { return x + y; };
//
//      // é aproximadamente:
//      class __lambda {
//          int x;                                        // ← a captura
//      public:
//          auto operator()(int y) const { return x + y; }
//      };
//
//  Entender isso responde sozinho a maioria das perguntas: por que `mutable`
//  existe, por que capturar por referência é perigoso, e por que `[=]` com
//  `this` é a armadilha mais comum do C++ moderno.
//
//  Tópicos:
//  • A lambda é uma classe — provado com sizeof
//  • Por valor, por referência, e a tabela de riscos
//  • `mutable` e o `operator() const`
//  • Init-capture (C++14): capturar por move, e capturar expressões
//  • ⚠ `[=]` captura `this` por PONTEIRO — a armadilha nº 1
//  • `[*this]` (C++17) e a correção
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <string>
#include <vector>

namespace demo_capturas {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A lambda é uma classe — e dá para medir
// ─────────────────────────────────────────────────────────────────────────────
void demo_e_uma_classe() {
    fmt::print("\n── 1.1 A lambda é uma classe ──\n");

    int         a = 1;
    double      b = 2.0;
    std::string s = "texto";

    auto sem_captura   = [] { return 0; };
    auto captura_int   = [a] { return a; };
    auto captura_dois  = [a, b] { return a + b; };
    auto captura_ref   = [&a] { return a; };
    auto captura_str   = [s] { return s.size(); };

    fmt::print("      sizeof([]{{}})           = {}  ← classe VAZIA (EBO se aplica)\n",
               sizeof(sem_captura));
    fmt::print("      sizeof([a]{{}})          = {}  = sizeof(int)\n",
               sizeof(captura_int));
    fmt::print("      sizeof([a,b]{{}})        = {}  = int + double + padding\n",
               sizeof(captura_dois));
    fmt::print("      sizeof([&a]{{}})         = {}  = um ponteiro\n",
               sizeof(captura_ref));
    fmt::print("      sizeof([s]{{}})          = {} = sizeof(std::string)\n",
               sizeof(captura_str));

    fmt::print("\n      Cada captura é um MEMBRO da classe gerada.\n");
    fmt::print("      Capturar por valor COPIA o objeto para dentro da lambda;\n");
    fmt::print("      capturar por referência guarda um ponteiro.\n");

    // E cada lambda tem um TIPO ÚNICO, mesmo com corpo idêntico
    auto f = [] { return 1; };
    auto g = [] { return 1; };
    fmt::print("      duas lambdas idênticas têm o mesmo tipo? {}\n",
               std::is_same_v<decltype(f), decltype(g)>);
    fmt::print("      ↑ por isso `std::function` existe: para guardar lambdas de\n");
    fmt::print("        tipos diferentes numa mesma variável (ver demo_function).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. As formas de capturar
// ─────────────────────────────────────────────────────────────────────────────
void demo_formas() {
    fmt::print("\n── 1.2 As formas de capturar ──\n");

    int x = 10, y = 20;

    auto por_valor      = [x]        { return x; };          // cópia de x
    auto por_ref        = [&x]       { return x; };          // referência a x
    auto tudo_valor     = [=]        { return x + y; };      // cópia de tudo usado
    auto tudo_ref       = [&]        { return x + y; };      // referência a tudo
    auto misto          = [=, &y]    { return x + y; };      // x por valor, y por ref
    auto misto2         = [&, x]     { return x + y; };      // tudo por ref, x por valor

    fmt::print("      antes:  x={} y={}\n", x, y);
    fmt::print("        [x]      → {}\n", por_valor());
    fmt::print("        [&x]     → {}\n", por_ref());
    fmt::print("        [=]      → {}\n", tudo_valor());
    fmt::print("        [&]      → {}\n", tudo_ref());

    x = 100; y = 200;
    fmt::print("      após x=100 y=200:\n");
    fmt::print("        [x]      → {}   ← congelou no valor da CRIAÇÃO\n", por_valor());
    fmt::print("        [&x]     → {} ← enxerga a mudança\n", por_ref());
    fmt::print("        [=]      → {}  ← congelado\n", tudo_valor());
    fmt::print("        [&]      → {} ← atualizado\n", tudo_ref());
    fmt::print("        [=, &y]  → {}  ← x congelado (10), y atualizado (200)\n", misto());
    fmt::print("        [&, x]   → {}  ← x congelado (10), y atualizado (200)\n", misto2());

    fmt::print("\n      | forma  | o que faz                          | risco    |\n");
    fmt::print("      |--------|------------------------------------|----------|\n");
    fmt::print("      | [x]    | copia x                            | nenhum   |\n");
    fmt::print("      | [&x]   | referencia x                       | dangling |\n");
    fmt::print("      | [=]    | copia tudo que USA (+ this por ptr)| ⚠ this   |\n");
    fmt::print("      | [&]    | referencia tudo que usa            | dangling |\n");
    fmt::print("      | [x=e]  | init-capture: cria membro do zero   | nenhum   |\n");

    fmt::print("\n      Recomendação: prefira capturas EXPLÍCITAS. `[=]` e `[&]`\n");
    fmt::print("      economizam digitação e escondem o que a lambda depende —\n");
    fmt::print("      inclusive o `this`, que é a armadilha da seção 1.5.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. `mutable` — o operator() é const por padrão
// ─────────────────────────────────────────────────────────────────────────────
void demo_mutable() {
    fmt::print("\n── 1.3 mutable ──\n");

    int contador = 0;

    // Sem `mutable`, o operator() é const → não dá para modificar a CÓPIA
    // auto errado = [contador] { return ++contador; };   // ✗ não compila

    auto com_mutable = [contador]() mutable { return ++contador; };
    auto por_ref     = [&contador]          { return ++contador; };

    // ⚠ As chamadas são feitas em linhas SEPARADAS de propósito.
    //   `f(g(), g(), g())` tem ordem de avaliação NÃO ESPECIFICADA: o GCC
    //   avalia da direita para a esquerda e imprimiria "3 2 1". Sempre que o
    //   efeito colateral importa, force a ordem com variáveis nomeadas.
    const int m1 = com_mutable();
    const int m2 = com_mutable();
    const int m3 = com_mutable();
    fmt::print("      [c]() mutable  → {} {} {}   (a CÓPIA interna avança)\n", m1, m2, m3);
    fmt::print("      contador externo continua = {}\n", contador);

    const int r1 = por_ref();
    const int r2 = por_ref();
    const int r3 = por_ref();
    fmt::print("      [&c]           → {} {} {}   (o ORIGINAL avança)\n", r1, r2, r3);
    fmt::print("      contador externo agora = {}\n", contador);

    fmt::print("\n      ⚠  as três chamadas acima estão em linhas separadas de\n");
    fmt::print("         propósito: `print(\"{{}} {{}} {{}}\", f(), f(), f())` tem ordem\n");
    fmt::print("         de avaliação NÃO ESPECIFICADA. O GCC avalia da direita\n");
    fmt::print("         para a esquerda e imprimiria 3 2 1. Quando o efeito\n");
    fmt::print("         colateral importa, use variáveis nomeadas.\n");

    fmt::print("\n      `mutable` remove o `const` do `operator()` gerado:\n");
    fmt::print("        auto operator()()       {{ ... }}   // com mutable\n");
    fmt::print("        auto operator()() const {{ ... }}   // sem (o padrão)\n");
    fmt::print("      A cópia capturada pertence à lambda, e o estado PERSISTE\n");
    fmt::print("      entre chamadas — a lambda vira um functor com estado.\n");

    fmt::print("\n      ⚠  uma lambda `mutable` não é copiável-e-independente por\n");
    fmt::print("         acidente: copiar a lambda copia o estado no ponto da\n");
    fmt::print("         cópia. É o que quebra o gerador de Fibonacci em views\n");
    fmt::print("         (ver src/ranges/demo_views.hpp, 1.4).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Init-capture (C++14) — capturar por move e capturar expressões
//
//     Antes do C++14, só dava para capturar variáveis existentes, e sempre por
//     cópia ou referência. Não havia como MOVER algo para dentro de uma lambda
//     — o que impedia capturar um `unique_ptr`.
// ─────────────────────────────────────────────────────────────────────────────
void demo_init_capture() {
    fmt::print("\n── 1.4 Init-capture (C++14) ──\n");

    // (a) Capturar por MOVE: essencial para tipos move-only
    auto recurso = std::make_unique<std::string>("dados caros");
    auto consome = [r = std::move(recurso)] {
        return r ? *r : std::string{"(vazio)"};
    };
    fmt::print("      [r = std::move(ptr)] → '{}'\n", consome());
    fmt::print("      o unique_ptr original agora é {}\n",
               recurso ? "válido" : "nulo (movido para dentro da lambda)");

    // (b) Capturar uma EXPRESSÃO, não uma variável
    const std::vector<int> v{1, 2, 3, 4, 5};
    auto soma_pre_calculada = [total = v.size() * 10] { return total; };
    fmt::print("      [total = v.size()*10] → {} (calculado UMA vez, na criação)\n",
               soma_pre_calculada());

    // (c) Renomear e reduzir o escopo capturado
    struct Config { int timeout; std::string host; int porta; };
    const Config cfg{30, "exemplo.com", 443};
    auto so_o_host = [h = cfg.host] { return h; };      // captura só o campo
    fmt::print("      [h = cfg.host] → '{}' (não copiou o Config inteiro)\n",
               so_o_host());

    // (d) Init-capture por referência, quando você quer nomear
    int contador = 0;
    auto incrementa = [&n = contador] { return ++n; };
    incrementa(); incrementa();
    fmt::print("      [&n = contador] → contador = {}\n", contador);

    fmt::print("\n      Init-capture resolve quatro problemas de uma vez:\n");
    fmt::print("        • capturar tipos move-only (unique_ptr, thread, ...)\n");
    fmt::print("        • pré-calcular uma vez, em vez de a cada chamada\n");
    fmt::print("        • capturar um MEMBRO sem capturar o objeto inteiro\n");
    fmt::print("        • dar um nome curto a algo com nome longo\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. ⚠ A armadilha nº 1: `[=]` captura `this` por PONTEIRO
//
//     Dentro de um método, `[=]` NÃO copia os membros. Ele copia o `this` —
//     que é um ponteiro. Se a lambda sobreviver ao objeto, todos os acessos a
//     membros são use-after-free.
//
//     Isso é tão traiçoeiro que o C++20 DEPRECOU a captura implícita de `this`
//     via `[=]`.
// ─────────────────────────────────────────────────────────────────────────────
class Servico {
public:
    explicit Servico(std::string nome) : nome_(std::move(nome)) {}

    // ✗ PERIGOSO: a lambda guarda `this`, não `nome_`
    std::function<std::string()> obter_saudacao_perigosa() const {
        return [this] { return "olá de " + nome_; };
        //      ↑ equivale a `[=]` para membros: captura o PONTEIRO
    }

    // ✓ SEGURO (C++17): `[*this]` copia o OBJETO inteiro para dentro da lambda
    std::function<std::string()> obter_saudacao_segura() const {
        return [*this] { return "olá de " + nome_; };
    }

    // ✓ SEGURO e mais barato: capture só o que precisa, por init-capture
    std::function<std::string()> obter_saudacao_minima() const {
        return [n = nome_] { return "olá de " + n; };
    }

private:
    std::string nome_;
};

void demo_this() {
    fmt::print("\n── 1.5 ⚠ A armadilha do `this` ──\n");

    std::function<std::string()> segura, minima;

    {
        Servico s{"servidor-01"};
        // A lambda perigosa também seria criada aqui — e ficaria pendurada
        // quando `s` saísse de escopo. Não a guardamos de propósito.
        segura = s.obter_saudacao_segura();
        minima = s.obter_saudacao_minima();

        fmt::print("      dentro do escopo de `s`:\n");
        fmt::print("        perigosa → '{}'  (funciona ENQUANTO s existe)\n",
                   s.obter_saudacao_perigosa()());
    }

    fmt::print("      após `s` ser destruído:\n");
    fmt::print("        [*this] → '{}'  ✓ copiou o objeto\n",  segura());
    fmt::print("        [n=nome_] → '{}'  ✓ copiou só a string\n", minima());
    fmt::print("        [this]  → seria use-after-free (não executado aqui)\n");

    fmt::print("\n      O que acontece com `[=]` dentro de um método:\n");
    fmt::print("        [=] {{ return nome_; }}\n");
    fmt::print("          → NÃO copia `nome_`\n");
    fmt::print("          → copia `this`, e `nome_` vira `this->nome_`\n");
    fmt::print("      Por isso o C++20 DEPRECOU a captura implícita de `this`\n");
    fmt::print("      via `[=]` (o GCC avisa com -Wdeprecated).\n");

    fmt::print("\n      | captura   | copia          | seguro após o objeto morrer |\n");
    fmt::print("      |-----------|----------------|-----------------------------|\n");
    fmt::print("      | [this]    | o ponteiro     | NÃO                         |\n");
    fmt::print("      | [=]       | o ponteiro (!) | NÃO                         |\n");
    fmt::print("      | [*this]   | o OBJETO       | sim (C++17)                 |\n");
    fmt::print("      | [n=nome_] | só o membro    | sim, e é o mais barato      |\n");

    fmt::print("\n      Regra: se a lambda pode SOBREVIVER ao objeto (callback,\n");
    fmt::print("      thread, std::function guardada), nunca capture `this`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Dangling por referência — o caso geral
// ─────────────────────────────────────────────────────────────────────────────
std::function<int()> fabrica_perigosa() {
    int local = 42;
    return [&local] { return local; };      // ← `local` morre ao sair
}

std::function<int()> fabrica_segura() {
    int local = 42;
    return [local] { return local; };       // ← copiou
}

void demo_dangling() {
    fmt::print("\n── 1.6 Dangling por referência ──\n");

    auto segura = fabrica_segura();
    fmt::print("      fabrica_segura()   → {} ✓ (capturou por VALOR)\n", segura());

    fmt::print("      fabrica_perigosa() → capturou `local` por REFERÊNCIA;\n");
    fmt::print("                           a variável morre no `return`.\n");
    fmt::print("                           Chamá-la é use-after-free.\n");
    fmt::print("                           (o GCC avisa: -Wdangling-reference)\n");

    fmt::print("\n      A regra que decide entre `[x]` e `[&x]`:\n");
    fmt::print("        a lambda é usada e descartada AQUI, no mesmo escopo?\n");
    fmt::print("          → `[&]` é seguro e evita cópias (algoritmos da STL)\n");
    fmt::print("        a lambda é GUARDADA, devolvida ou passada a outra thread?\n");
    fmt::print("          → capture por VALOR, sempre\n");

    fmt::print("\n      Casos em que `[&]` é o certo:\n");
    fmt::print("        std::sort(v.begin(), v.end(), [&](auto& a, auto& b) {{...}});\n");
    fmt::print("        → o algoritmo consome a lambda antes de retornar\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_e_uma_classe();
    demo_formas();
    demo_mutable();
    demo_init_capture();
    demo_this();
    demo_dangling();
}

} // namespace demo_capturas
