// =============================================================================
//  src/linkage/demo_linkage.hpp
//
//  Os três tipos de linkage
//  ────────────────────────
//  "Linkage" responde a uma pergunta só: este nome pode se referir à MESMA
//  entidade que um nome igual em outro lugar?
//
//    EXTERNA  — sim, inclusive em outra unidade de tradução
//    INTERNA  — sim, mas só dentro desta unidade de tradução
//    NENHUMA  — não; o nome só existe onde foi declarado
//
//  Não confunda com ESCOPO (onde o nome é visível) nem com TEMPO DE VIDA
//  (quando o objeto existe). Uma variável `static` dentro de uma função tem
//  linkage NENHUMA, escopo de bloco e tempo de vida do programa inteiro.
//
//  Tópicos:
//  • A tabela de quem tem qual linkage
//  • static: a palavra com quatro significados
//  • Namespace anônimo — a forma moderna
//  • ⚠ static local: a inicialização thread-safe e seu custo
//  • extern e a declaração vs definição
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <thread>
#include <vector>

namespace demo_linkage {

// ─────────────────────────────────────────────────────────────────────────────
//  1. A tabela
// ─────────────────────────────────────────────────────────────────────────────
void demo_tabela() {
    fmt::print("\n── 3.1 Quem tem qual linkage ──\n");

    fmt::print("      | declaração em escopo de namespace   | linkage  |\n");
    fmt::print("      |-------------------------------------|----------|\n");
    fmt::print("      | int x;                              | EXTERNA  |\n");
    fmt::print("      | void f() {{}}                         | EXTERNA  |\n");
    fmt::print("      | inline void f() {{}}                  | EXTERNA  |\n");
    fmt::print("      | static int x;                       | interna  |\n");
    fmt::print("      | static void f() {{}}                  | interna  |\n");
    fmt::print("      | const int x = 1;                    | interna  | ← surpresa\n");
    fmt::print("      | constexpr int x = 1;                | interna  | ← implica const\n");
    fmt::print("      | inline constexpr int x = 1;         | EXTERNA  |\n");
    fmt::print("      | namespace {{ int x; }}                | interna  |\n");
    fmt::print("      | extern const int x = 1;             | EXTERNA  |\n");
    fmt::print("      | typedef / using                     | nenhuma  |\n");
    fmt::print("      | enum sem nome                       | nenhuma  |\n\n");

    fmt::print("      | declaração em escopo de BLOCO       | linkage  |\n");
    fmt::print("      |-------------------------------------|----------|\n");
    fmt::print("      | int x;              (local comum)   | nenhuma  |\n");
    fmt::print("      | static int x;       (local estático)| nenhuma  | ← mas dura\n");
    fmt::print("      | extern int x;                       | EXTERNA  |   o programa todo\n\n");

    fmt::print("      A linha que mais surpreende é `const int x = 1;`. Em C ela\n");
    fmt::print("      teria linkage EXTERNA; em C++, interna. A razão é prática:\n");
    fmt::print("      permite pôr constantes em cabeçalhos sem \"multiple\n");
    fmt::print("      definition\" — cada TU ganha a sua cópia, e como são const\n");
    fmt::print("      e iguais, ninguém percebe.\n");
    fmt::print("      O preço é o que a seção 2.1 mediu: endereços diferentes.\n");
    fmt::print("      Se você precisa de UM objeto, escreva `inline constexpr`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. `static`: uma palavra, quatro significados
// ─────────────────────────────────────────────────────────────────────────────
namespace exemplo_static {

// `[[maybe_unused]]`: existe aqui só para ilustrar a forma (1) do `static`.
// Sem ele o GCC avisa "defined but not used" — e o aviso estaria certo.
[[maybe_unused]] static int global_estatica = 0;   // (1) linkage interna

struct Classe {
    static int   membro;              // (2) um por CLASSE, não por objeto
    static void  metodo() {}          // (3) sem `this`
};
inline int Classe::membro = 0;        // C++17: definição inline

inline int contador_de_chamadas() {
    static int n = 0;                 // (4) local, mas vive o programa todo
    return ++n;
}

}  // namespace exemplo_static

void demo_static() {
    fmt::print("\n── 3.2 `static`: quatro significados ──\n");

    fmt::print("      (1) em escopo de namespace → LINKAGE INTERNA\n");
    fmt::print("          static int g;    // só esta TU enxerga\n\n");

    fmt::print("      (2) membro de classe → UM por classe, não por objeto\n");
    fmt::print("          struct C {{ static int membro; }};\n");
    fmt::print("          sizeof(Classe) = {} — o membro estático NÃO está no\n",
               sizeof(exemplo_static::Classe));
    fmt::print("          objeto. (1 byte porque a classe ficou vazia.)\n\n");

    fmt::print("      (3) método de classe → sem `this`, chamável sem objeto\n");
    fmt::print("          Classe::metodo();\n\n");

    fmt::print("      (4) local de função → TEMPO DE VIDA do programa,\n");
    fmt::print("          linkage NENHUMA, escopo do bloco\n");
    for (int i = 0; i < 3; ++i)
        fmt::print("          chamada {} → contador = {}\n",
                   i + 1, exemplo_static::contador_de_chamadas());

    fmt::print("\n      Quatro conceitos ortogonais na mesma palavra. É herança do\n");
    fmt::print("      C, e o C++ não pôde consertar sem quebrar tudo. Para o\n");
    fmt::print("      significado (1), prefira NAMESPACE ANÔNIMO — mesmo efeito,\n");
    fmt::print("      sem ambiguidade, e funciona também para tipos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Namespace anônimo
// ─────────────────────────────────────────────────────────────────────────────
void demo_anonimo() {
    fmt::print("\n── 3.3 Namespace anônimo ──\n");

    fmt::print("        namespace {{\n");
    fmt::print("            int      variavel;\n");
    fmt::print("            void     funcao() {{}}\n");
    fmt::print("            struct   Tipo {{}};      // ← `static` NÃO faz isto\n");
    fmt::print("        }}\n\n");

    fmt::print("      O compilador dá ao namespace um nome único por TU e insere\n");
    fmt::print("      um `using namespace` implícito. Consequências:\n");
    fmt::print("        • os nomes são inacessíveis de outras TUs;\n");
    fmt::print("        • funciona para TIPOS, o que `static` não faz;\n");
    fmt::print("        • os símbolos ainda aparecem no .o (manglados com o nome\n");
    fmt::print("          único) — `static` gera símbolo local; na prática o\n");
    fmt::print("          linker resolve os dois casos igual.\n\n");

    fmt::print("      ⚠  Nunca num CABEÇALHO. Cada TU que incluir ganha a sua\n");
    fmt::print("         cópia — o mesmo bug de 2.2, agora com tipos: dois tipos\n");
    fmt::print("         com o mesmo nome e identidades diferentes.\n\n");

    fmt::print("      Onde usar: no .cpp, para tudo que é detalhe de\n");
    fmt::print("      implementação. É documentação (\"isto não é API\") e ajuda o\n");
    fmt::print("      otimizador — ele sabe que ninguém de fora chama, e pode\n");
    fmt::print("      inlinar ou eliminar à vontade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ O static local e a inicialização thread-safe
// ─────────────────────────────────────────────────────────────────────────────
void demo_static_local() {
    fmt::print("\n── 3.4 ⚠ static local e threads ──\n");

    fmt::print("      Desde o C++11, a inicialização de um static local é\n");
    fmt::print("      GARANTIDAMENTE thread-safe: se duas threads chegarem\n");
    fmt::print("      juntas, uma inicializa e a outra ESPERA.\n\n");

    static int inicializacoes = 0;
    auto singleton = [&]() -> const std::string& {
        static const std::string s = (++inicializacoes, "inicializado uma vez");
        return s;
    };

    {
        std::vector<std::jthread> ts;
        for (int i = 0; i < 8; ++i) ts.emplace_back([&] { (void)singleton(); });
    }
    fmt::print("      8 threads chamaram ao mesmo tempo → {} inicialização(ões)\n",
               inicializacoes);

    fmt::print("\n      É isto que torna o \"Meyers singleton\" correto sem mutex:\n");
    fmt::print("        Config& instancia() {{ static Config c; return c; }}\n");
    fmt::print("      Ver src/design-of-pattern/singleton/.\n\n");

    fmt::print("      O custo: o compilador emite um guard variable e uma\n");
    fmt::print("      checagem atômica em CADA chamada (`__cxa_guard_acquire`).\n");
    fmt::print("      Depois da primeira, é uma leitura atômica relaxed — barato,\n");
    fmt::print("      mas não zero. Num laço quente, iça a chamada para fora.\n\n");

    fmt::print("      ⚠  A garantia é sobre a INICIALIZAÇÃO, não sobre o uso. Se\n");
    fmt::print("         várias threads MUTAM o objeto depois, você ainda precisa\n");
    fmt::print("         de sincronização (ver src/concurrency2/).\n\n");

    fmt::print("      ⚠  E ela não resolve a ordem de DESTRUIÇÃO. Static locals\n");
    fmt::print("         são destruídos na ordem inversa da inicialização, o que\n");
    fmt::print("         é previsível; já globais de TUs diferentes têm ordem\n");
    fmt::print("         INDEFINIDA entre si — o \"static initialization order\n");
    fmt::print("         fiasco\", que o static local justamente evita.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. extern
// ─────────────────────────────────────────────────────────────────────────────
void demo_extern() {
    fmt::print("\n── 3.5 `extern` ──\n");

    fmt::print("      `extern` diz \"isto é uma DECLARAÇÃO, não uma definição\":\n\n");
    fmt::print("        // config.hpp\n");
    fmt::print("        extern int nivel_de_log;      // declara\n\n");
    fmt::print("        // config.cpp\n");
    fmt::print("        int nivel_de_log = 3;         // define, UMA vez\n\n");

    fmt::print("      Era o único jeito de ter uma global compartilhada antes do\n");
    fmt::print("      C++17. Hoje `inline int nivel_de_log = 3;` no cabeçalho faz\n");
    fmt::print("      o mesmo em uma linha — e sem o risco de esquecer o .cpp.\n\n");

    fmt::print("      `extern` ainda é necessário para:\n");
    fmt::print("        • `extern const int k;`  → forçar linkage EXTERNA numa\n");
    fmt::print("          constante (que seria interna por padrão);\n");
    fmt::print("        • `extern \"C\"`           → linkage de LINGUAGEM, coisa\n");
    fmt::print("          completamente diferente — seção 4;\n");
    fmt::print("        • `extern template`      → suprimir a instanciação numa\n");
    fmt::print("          TU e pegá-la de outra, para acelerar o build.\n\n");

    fmt::print("      `extern template std::vector<MeuTipo>;` num cabeçalho, mais\n");
    fmt::print("      `template std::vector<MeuTipo>;` num .cpp, instancia uma vez\n");
    fmt::print("      só em vez de N. Vale para templates grandes e muito usados.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_tabela();
    demo_static();
    demo_anonimo();
    demo_static_local();
    demo_extern();
}

}  // namespace demo_linkage
