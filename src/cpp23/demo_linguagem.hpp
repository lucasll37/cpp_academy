// =============================================================================
//  src/cpp23/demo_linguagem.hpp
//
//  As novidades de LINGUAGEM do C++23 que o GCC 13 tem
//  ───────────────────────────────────────────────────
//  O C++23 é uma revisão de consolidação: poucas features grandes, muitas
//  arestas aparadas. Estas são as que este toolchain suporta.
//
//  Tópicos:
//  • if consteval — e por que ele existe apesar de is_constant_evaluated
//  • Subscript multidimensional
//  • static operator() e static operator[]
//  • Os sufixos `uz` e `z`
//  • [[assume]] — a promessa que o otimizador acredita
//  • Escapes nomeados de caractere
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_linguagem {

// ─────────────────────────────────────────────────────────────────────────────
//  1. if consteval
//
//     Já apareceu em src/compile_time/demo_consteval.hpp. Aqui o foco é POR QUE
//     ele foi necessário, dado que `std::is_constant_evaluated()` já existia.
// ─────────────────────────────────────────────────────────────────────────────
consteval int so_em_compilacao(int x) { return x * 2; }

// ✓ Com `if consteval`, dá para chamar uma `consteval` do ramo de compilação
constexpr int hibrido(int x) {
    if consteval {
        return so_em_compilacao(x);      // legal: o contexto é imediato
    } else {
        return x * 2;                    // caminho de runtime
    }
}

void demo_if_consteval() {
    fmt::print("\n── 2.1 if consteval ──\n");

    constexpr int em_compilacao = hibrido(21);
    static_assert(em_compilacao == 42);

    int runtime = 21;
    const int em_execucao = hibrido(runtime);

    fmt::print("      hibrido(21) constexpr → {}\n", em_compilacao);
    fmt::print("      hibrido(variável)     → {}\n", em_execucao);

    fmt::print("\n      Por que `is_constant_evaluated()` não bastava:\n");
    fmt::print("        constexpr int f(int x) {{\n");
    fmt::print("            if (std::is_constant_evaluated())\n");
    fmt::print("                return so_em_compilacao(x);  // ✗ NÃO COMPILA\n");
    fmt::print("            return x * 2;\n");
    fmt::print("        }}\n");
    fmt::print("      O ramo do `if` NORMAL não é um contexto imediato, então\n");
    fmt::print("      chamar uma `consteval` ali é erro — mesmo que o ramo só\n");
    fmt::print("      execute em compilação.\n");
    fmt::print("      O `if consteval` CRIA esse contexto. É a diferença que\n");
    fmt::print("      justifica a palavra-chave nova.\n");

    fmt::print("\n      E as duas armadilhas que ele elimina:\n");
    fmt::print("        if constexpr (std::is_constant_evaluated())  // SEMPRE true\n");
    fmt::print("        if (std::is_constant_evaluated())            // ok, mas\n");
    fmt::print("                                                     // não é ctx imediato\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Subscript multidimensional
//
//     Já demonstrado em src/operators/demo_chamada.hpp, 4.3. Aqui, a armadilha
//     de compatibilidade.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t L, std::size_t C>
class Matriz {
public:
    constexpr T&       operator[](std::size_t i, std::size_t j)       { return d_[i * C + j]; }
    constexpr const T& operator[](std::size_t i, std::size_t j) const { return d_[i * C + j]; }
private:
    std::array<T, L * C> d_{};
};

void demo_subscript() {
    fmt::print("\n── 2.2 Subscript multidimensional ──\n");

    Matriz<int, 2, 3> m;
    m[0, 0] = 1; m[0, 1] = 2; m[0, 2] = 3;
    m[1, 0] = 4; m[1, 1] = 5; m[1, 2] = 6;

    fmt::print("      m[i, j]:\n");
    for (std::size_t i = 0; i < 2; ++i) {
        fmt::print("        ");
        for (std::size_t j = 0; j < 3; ++j) fmt::print("{:3}", m[i, j]);
        fmt::print("\n");
    }

    // Funciona em constexpr também
    constexpr auto preenchida = [] {
        Matriz<int, 2, 2> x;
        x[0, 0] = 10; x[1, 1] = 20;
        return x;
    }();
    static_assert(preenchida[1, 1] == 20);
    fmt::print("      e em constexpr: preenchida[1,1] = {}\n", preenchida[1, 1]);

    fmt::print("\n      ⚠  A ARMADILHA DE COMPATIBILIDADE:\n");
    fmt::print("         em C++20, `a[i, j]` era o operador VÍRGULA:\n");
    fmt::print("           avalia `i`, DESCARTA, e indexa por `j`\n");
    fmt::print("         O C++20 depreciou esse uso; o C++23 reciclou a sintaxe.\n");
    fmt::print("         Código antigo com `a[i, j]` MUDA DE COMPORTAMENTO ao\n");
    fmt::print("         recompilar em C++23 — silenciosamente, se `a` tiver o\n");
    fmt::print("         operador novo.\n");
    fmt::print("         Compile com -Wcomma-subscript para o GCC avisar.\n");

    // Demonstração do operador vírgula ainda funcionando em array cru
    const int arr[5]{10, 20, 30, 40, 50};
    fmt::print("\n      num array CRU, `arr[1, 3]` ainda é o operador vírgula:\n");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomma-subscript"
#pragma GCC diagnostic ignored "-Wdeprecated"
    fmt::print("        arr[1, 3] = {}  ← indexou por 3, ignorou o 1\n", arr[1, 3]);
#pragma GCC diagnostic pop
    fmt::print("        (arrays crus não podem ter operator[] customizado)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. static operator() e static operator[]
// ─────────────────────────────────────────────────────────────────────────────
struct EhPar {
    static constexpr bool operator()(int x) { return x % 2 == 0; }
};

struct TabelaQuadrados {
    static constexpr int operator[](std::size_t i) { return static_cast<int>(i * i); }
};

void demo_static_operator() {
    fmt::print("\n── 2.3 static operator() e operator[] ──\n");

    static_assert(EhPar{}(4));
    static_assert(!EhPar{}(5));
    static_assert(TabelaQuadrados{}[7] == 49);

    fmt::print("      EhPar{{}}(4)            = {}\n", EhPar{}(4));
    fmt::print("      TabelaQuadrados{{}}[7]  = {}\n", TabelaQuadrados{}[7]);

    // E dá para chamar sem instância nenhuma
    fmt::print("      EhPar::operator()(10) = {}  ← sem instanciar\n",
               EhPar::operator()(10));

    fmt::print("\n      O ganho: um functor SEM ESTADO não precisa do `this`.\n");
    fmt::print("      Marcá-lo `static` remove o parâmetro implícito da chamada.\n");
    fmt::print("      Em código genérico muito quente, é um argumento a menos\n");
    fmt::print("      por invocação.\n");

    fmt::print("\n      É por isso que os adaptadores de <ranges> do C++23 usam\n");
    fmt::print("      `static operator()` — eles são chamados milhões de vezes\n");
    fmt::print("      e nenhum tem estado.\n");

    fmt::print("\n      ⚠  `static` e `const` são mutuamente exclusivos aqui:\n");
    fmt::print("         `static bool operator()(int) const` não compila —\n");
    fmt::print("         não há `this` para ser const.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Os sufixos `uz` e `z`
//
//     `size_t` não tinha sufixo de literal. Isso obrigava a `static_cast` ou a
//     comparações com sinal misto — a origem de muitos `-Wsign-compare`.
// ─────────────────────────────────────────────────────────────────────────────
void demo_sufixo_uz() {
    fmt::print("\n── 2.4 Os sufixos `uz` e `z` ──\n");

    const std::vector<int> v{10, 20, 30};

    // Antes: o literal `0` é int, e comparar com size() gera -Wsign-compare
    fmt::print("      antes do C++23:\n");
    fmt::print("        for (std::size_t i = 0; i < v.size(); ++i)   // ok, verboso\n");
    fmt::print("        for (auto i = 0; i < v.size(); ++i)          // ⚠ -Wsign-compare\n");

    // Agora: `0uz` já é size_t
    fmt::print("\n      com C++23:\n");
    for (auto i = 0uz; i < v.size(); ++i) {
        fmt::print("        v[{}] = {}\n", i, v[i]);
    }

    static_assert(std::is_same_v<decltype(0uz), std::size_t>);
    static_assert(std::is_same_v<decltype(0z),  std::make_signed_t<std::size_t>>);

    fmt::print("\n      tipos: `0uz` é size_t ✓ | `0z` é ptrdiff_t (com sinal) ✓\n");
    fmt::print("      É uma feature pequena que remove uma classe inteira de\n");
    fmt::print("      warnings e de `static_cast` decorativos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. [[assume]] — a promessa que o otimizador acredita
//
//     Diz ao compilador "esta condição é verdadeira, pode contar com isso".
//     Se ela for FALSA em runtime, o comportamento é indefinido.
// ─────────────────────────────────────────────────────────────────────────────
int dividir_por_potencia_de_dois(int valor, int expoente) {
    [[assume(expoente >= 0 && expoente < 31)]];
    // Com a promessa, o compilador pode gerar um shift em vez de uma divisão,
    // sem precisar tratar expoente negativo ou grande demais.
    return valor >> expoente;
}

void demo_assume() {
    fmt::print("\n── 2.5 [[assume]] ──\n");

    fmt::print("      dividir_por_potencia_de_dois(1024, 3) = {}\n",
               dividir_por_potencia_de_dois(1024, 3));

    fmt::print("\n      A sintaxe:\n");
    fmt::print("        [[assume(cond)]];\n");
    fmt::print("      A promessa: `cond` é VERDADEIRA. O compilador pode\n");
    fmt::print("      otimizar contando com isso — eliminar checagens, escolher\n");
    fmt::print("      instruções mais estreitas, remover ramos.\n");

    fmt::print("\n      ⚠  se `cond` for FALSA em runtime, é UNDEFINED BEHAVIOR.\n");
    fmt::print("         `[[assume]]` é uma AFIRMAÇÃO ao compilador, não uma\n");
    fmt::print("         verificação. O oposto de um `assert`:\n");
    fmt::print("           assert(x > 0)      → verifica, aborta se falso\n");
    fmt::print("           [[assume(x > 0)]]  → NÃO verifica, UB se falso\n");

    fmt::print("\n      A expressão NÃO é avaliada — efeitos colaterais não\n");
    fmt::print("      acontecem:\n");
    fmt::print("        [[assume(++i > 0)]];   // `i` NÃO é incrementado\n");

    fmt::print("\n      Use com muita parcimônia, e sempre com um assert em\n");
    fmt::print("      debug para validar a premissa:\n");
    fmt::print("        assert(e >= 0 && e < 31);\n");
    fmt::print("        [[assume(e >= 0 && e < 31)]];\n");
    fmt::print("      Assim o build de debug PROVA o que o de release ASSUME.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Escapes nomeados de caractere
// ─────────────────────────────────────────────────────────────────────────────
void demo_escapes() {
    fmt::print("\n── 2.6 Escapes nomeados ──\n");

    // \N{NOME} usa o nome oficial do caractere no Unicode
    const char* seta   = "\N{RIGHTWARDS ARROW}";
    const char* grau   = "\N{DEGREE SIGN}";
    const char* alfa   = "\N{GREEK SMALL LETTER ALPHA}";

    fmt::print("      \\N{{RIGHTWARDS ARROW}}       → {}\n", seta);
    fmt::print("      \\N{{DEGREE SIGN}}            → {}\n", grau);
    fmt::print("      \\N{{GREEK SMALL LETTER ALPHA}} → {}\n", alfa);

    fmt::print("\n      Antes: `\\u2192`, e você precisava saber o número.\n");
    fmt::print("      Agora: o nome OFICIAL do Unicode, e o compilador valida —\n");
    fmt::print("      um nome errado é erro de compilação, não um caractere\n");
    fmt::print("      errado que só aparece na tela.\n");

    fmt::print("\n      ⚠  o nome tem que ser EXATO, em maiúsculas, como no\n");
    fmt::print("         padrão Unicode. `\\N{{seta}}` não compila.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_if_consteval();
    demo_subscript();
    demo_static_operator();
    demo_sufixo_uz();
    demo_assume();
    demo_escapes();
}

} // namespace demo_linguagem
