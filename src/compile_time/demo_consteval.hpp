// =============================================================================
//  src/compile_time/demo_consteval.hpp
//
//  consteval e constinit — as garantias que faltavam
//  ─────────────────────────────────────────────────
//  `constexpr` diz "pode rodar na compilação". Isso deixa duas perguntas sem
//  resposta, e o C++20 acrescentou uma palavra-chave para cada uma:
//
//    consteval  — "SEMPRE roda na compilação". Chamar com argumento de runtime
//                 é erro de compilação. São as "immediate functions".
//    constinit  — "a inicialização acontece na compilação". Não torna a
//                 variável const; só garante que ela não tem inicialização
//                 dinâmica.
//
//  E o C++23 fechou o círculo com `if consteval`, que permite a uma função
//  saber em qual dos dois mundos ela está executando.
//
//  Tópicos:
//  • consteval: a garantia forte
//  • Por que consteval não pode ser chamada de constexpr
//  • constinit e o "static initialization order fiasco"
//  • if consteval (C++23) e is_constant_evaluated
//  • Validação em compilação: a aplicação mais útil de consteval
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <string_view>
#include <type_traits>

namespace demo_consteval {

// ─────────────────────────────────────────────────────────────────────────────
//  1. consteval — a garantia forte
// ─────────────────────────────────────────────────────────────────────────────
constexpr int quadrado_constexpr(int x) { return x * x; }   // PODE
consteval int quadrado_consteval(int x) { return x * x; }   // SEMPRE

void demo_garantia() {
    fmt::print("\n── 2.1 consteval — a garantia forte ──\n");

    int runtime = 7;

    // constexpr aceita os dois mundos
    fmt::print("      quadrado_constexpr(7)         = {} (constante)\n",
               quadrado_constexpr(7));
    fmt::print("      quadrado_constexpr(variável)  = {} (runtime, e tudo bem)\n",
               quadrado_constexpr(runtime));

    // consteval só aceita constantes
    constexpr int c = quadrado_consteval(7);
    fmt::print("      quadrado_consteval(7)         = {} (sempre na compilação)\n", c);
    // quadrado_consteval(runtime);   // ✗ erro: "call to consteval function is
    //                                //    not a constant expression"
    fmt::print("      quadrado_consteval(variável)  → NÃO COMPILA\n");

    fmt::print("\n      | palavra     | avaliação em compilação | argumento runtime |\n");
    fmt::print("      |-------------|-------------------------|-------------------|\n");
    fmt::print("      | (nenhuma)   | não                     | ok                |\n");
    fmt::print("      | constexpr   | permitida               | ok                |\n");
    fmt::print("      | consteval   | OBRIGATÓRIA             | ERRO              |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A regra que confunde: consteval dentro de constexpr
//
//     Uma função `constexpr` PODE ser chamada em runtime. Se ela chamasse uma
//     `consteval`, essa chamada teria que acontecer em runtime — o que é
//     proibido por definição. Então o compilador recusa já na definição.
// ─────────────────────────────────────────────────────────────────────────────
consteval int dobro_imediato(int x) { return x * 2; }

// ✗ Não compila: `f` pode ser chamada em runtime, e aí `dobro_imediato(x)`
//   seria uma chamada de consteval em runtime.
// constexpr int f(int x) { return dobro_imediato(x); }

// ✓ Compila: dentro de outra consteval, o contexto já é garantido
consteval int g(int x) { return dobro_imediato(x) + 1; }

// ✓ Compila: o argumento é constante literal, não o parâmetro
constexpr int h(int x) { return x + dobro_imediato(10); }

void demo_propagacao() {
    fmt::print("\n── 2.2 consteval dentro de constexpr ──\n");

    fmt::print("      consteval int dobro_imediato(int);\n\n");
    fmt::print("      ✗ constexpr int f(int x) {{ return dobro_imediato(x); }}\n");
    fmt::print("        → NÃO COMPILA: `f` pode ser chamada em runtime, e aí a\n");
    fmt::print("          chamada a `dobro_imediato` também seria de runtime.\n\n");
    fmt::print("      ✓ consteval int g(int x) {{ return dobro_imediato(x) + 1; }}\n");
    fmt::print("        → g(5) = {}  (o contexto imediato propaga)\n", g(5));
    fmt::print("      ✓ constexpr int h(int x) {{ return x + dobro_imediato(10); }}\n");
    fmt::print("        → h(1) = {}  (o argumento é literal, não o parâmetro)\n", h(1));

    fmt::print("\n      Regra: `consteval` é contagiosa para cima. Uma função que\n");
    fmt::print("      repassa um PARÂMETRO para uma consteval precisa ser\n");
    fmt::print("      consteval também.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. A aplicação mais útil: validação em compilação
//
//     Uma consteval que recebe a entrada e VALIDA. Se a validação falhar, o
//     compilador para — porque `throw` numa avaliação constexpr é erro de
//     compilação, não exceção.
//
//     É assim que `std::format` valida a string de formato em C++20.
// ─────────────────────────────────────────────────────────────────────────────

// Um "porta" que só aceita valores válidos, checados na compilação
class Porta {
public:
    consteval explicit Porta(int valor) : valor_(valor) {
        if (valor < 1 || valor > 65535) {
            // Numa avaliação em compilação, um throw que escapa é ERRO DE
            // COMPILAÇÃO. A mensagem do compilador cita esta linha.
            throw "porta fora do intervalo 1..65535";
        }
    }
    constexpr int valor() const { return valor_; }
private:
    int valor_;
};

// Validação de uma string de formato simplificada
consteval std::size_t contar_placeholders(std::string_view fmt_str) {
    std::size_t n = 0;
    for (std::size_t i = 0; i < fmt_str.size(); ++i) {
        if (fmt_str[i] != '{') continue;
        if (i + 1 < fmt_str.size() && fmt_str[i + 1] == '{') { ++i; continue; }  // escape
        const auto fecha = fmt_str.find('}', i);
        if (fecha == std::string_view::npos) {
            throw "chave '{' sem o '}' correspondente";     // erro de compilação
        }
        ++n;
        i = fecha;
    }
    return n;
}

void demo_validacao() {
    fmt::print("\n── 2.3 Validação em compilação ──\n");

    constexpr Porta http{80};
    constexpr Porta https{443};
    fmt::print("      Porta{{80}}  → {}\n", http.valor());
    fmt::print("      Porta{{443}} → {}\n", https.valor());

    // constexpr Porta invalida{70000};
    // → erro de compilação, citando a linha do `throw` no construtor:
    //   "'throw' is not a constant expression"
    fmt::print("      Porta{{70000}} → NÃO COMPILA (o throw no ctor barra)\n");

    static_assert(contar_placeholders("{} e {}")       == 2);
    static_assert(contar_placeholders("literal {{}} ") == 0);   // {{ é escape
    fmt::print("      contar_placeholders(\"{{}} e {{}}\")  = {}\n",
               contar_placeholders("{} e {}"));
    fmt::print("      contar_placeholders(\"sem nada\")  = {}\n",
               contar_placeholders("sem nada"));
    fmt::print("      contar_placeholders(\"quebrado {{\") → NÃO COMPILA\n");

    fmt::print("\n      Este é o mecanismo por trás do `std::format` do C++20:\n");
    fmt::print("      a string de formato é consteval, então\n");
    fmt::print("        std::format(\"{{:d}}\", \"texto\")\n");
    fmt::print("      falha na COMPILAÇÃO, e não com uma exceção em produção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. constinit — matando o static initialization order fiasco
//
//     O problema: variáveis globais de unidades de tradução diferentes são
//     inicializadas em ordem NÃO ESPECIFICADA. Se a inicialização de uma
//     depende de outra, o comportamento é indefinido — e varia com a ordem de
//     link.
//
//     `constinit` exige que a inicialização seja ESTÁTICA (feita na
//     compilação, gravada direto no binário). Aí não há ordem nenhuma a
//     respeitar: o valor já está lá quando o programa começa.
// ─────────────────────────────────────────────────────────────────────────────
constexpr int calcular_limite() {
    int r = 1;
    for (int i = 0; i < 10; ++i) r *= 2;
    return r;
}

// ✓ Inicialização estática garantida: o 1024 vai direto para a seção .data
constinit int limite_global = calcular_limite();

// ✗ Não compilaria: a inicialização dependeria de runtime
// constinit int agora = std::time(nullptr);

// Diferença importante: `constinit` NÃO implica `const`
void demo_constinit() {
    fmt::print("\n── 2.4 constinit ──\n");

    fmt::print("      constinit int limite_global = calcular_limite();\n");
    fmt::print("        valor inicial = {}\n", limite_global);

    limite_global = 2048;                     // ✓ constinit não é const!
    fmt::print("        após limite_global = 2048 → {}\n", limite_global);
    limite_global = calcular_limite();        // restaura, para o demo ser idempotente

    fmt::print("\n      | palavra   | inicialização | mutável depois |\n");
    fmt::print("      |-----------|---------------|----------------|\n");
    fmt::print("      | const     | qualquer      | não            |\n");
    fmt::print("      | constexpr | compilação    | não            |\n");
    fmt::print("      | constinit | COMPILAÇÃO    | SIM            |\n");

    fmt::print("\n      O problema que `constinit` resolve:\n");
    fmt::print("        // a.cpp\n");
    fmt::print("        Config config = carregar();      // ordem indefinida\n");
    fmt::print("        // b.cpp\n");
    fmt::print("        int porta = config.porta;        // pode ler lixo!\n");
    fmt::print("      Com `constinit`, o compilador RECUSA qualquer inicialização\n");
    fmt::print("      que não caiba na compilação — o fiasco vira erro de build.\n");
    fmt::print("      (a outra saída clássica é o Meyers singleton — ver\n");
    fmt::print("       src/solid/ocp/demo_registry.hpp)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Sabendo em qual mundo você está
//
//     Às vezes o algoritmo ótimo difere entre compilação e execução: em
//     runtime você quer o intrínseco do processador; em compilação, ele não
//     existe e é preciso um laço.
//
//     C++20 trouxe `std::is_constant_evaluated()`; C++23 trouxe `if consteval`,
//     que é mais seguro (ver a armadilha abaixo).
// ─────────────────────────────────────────────────────────────────────────────
constexpr int contar_bits(unsigned x) {
    if consteval {
        // Caminho de COMPILAÇÃO: laço simples, sem intrínsecos
        int n = 0;
        while (x) { n += static_cast<int>(x & 1u); x >>= 1; }
        return n;
    } else {
        // Caminho de EXECUÇÃO: o intrínseco do processador (uma instrução)
        return __builtin_popcount(x);
    }
}

// ── As duas armadilhas do is_constant_evaluated, medidas ───────────────────
//
//  ATENÇÃO: `std::is_constant_evaluated()` dentro de um `if` COMUM se comporta
//  como se espera — true quando a função roda na compilação, false quando roda
//  em runtime. Esse é justamente o uso correto da função.
constexpr int ice_normal(int) {
    if (std::is_constant_evaluated()) return 1;   // compilação
    else                              return 2;   // execução
}

//  Armadilha 1: com `if constexpr`, a CONDIÇÃO é ela própria avaliada em
//  compilação — então o ramo do `else` nunca é gerado, e o resultado é 1 mesmo
//  numa chamada de runtime.
//  O próprio GCC avisa disso com -Wtautological-compare ("always evaluates to
//  true in 'if constexpr'"). O aviso está CERTO — e é a lição. Silenciamos só
//  aqui, porque o erro é intencional e o demo existe para mostrá-lo.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtautological-compare"
constexpr int ice_if_constexpr(int) {
    if constexpr (std::is_constant_evaluated()) return 1;
    else                                        return 2;
}
#pragma GCC diagnostic pop

//  Armadilha 2: inicializar uma variável `const` local de tipo integral torna
//  aquele inicializador "manifestamente avaliado em constante" — `n` vira 13
//  mesmo quando a função é chamada em runtime, enquanto `m` (não-const) vira 17.
constexpr int ice_const_local() {
    const int n = std::is_constant_evaluated() ? 13 : 17;
    int       m = std::is_constant_evaluated() ? 13 : 17;
    return n * 100 + m;
}

void demo_if_consteval() {
    fmt::print("\n── 2.5 if consteval (C++23) ──\n");

    constexpr int em_compilacao = contar_bits(0b1011'0110);
    static_assert(em_compilacao == 5);

    unsigned valor = 0b1011'0110;
    const int em_runtime = contar_bits(valor);

    // Medições das armadilhas (a variável `zero` impede a avaliação constante)
    int zero = 0;
    constexpr int ice_em_compilacao  = ice_normal(0);
    const     int ice_em_runtime     = ice_normal(zero);
    constexpr int ifc_em_compilacao  = ice_if_constexpr(0);
    const     int ifc_em_runtime     = ice_if_constexpr(zero);
    int armadilha_const_local_rt = ice_const_local();   // NÃO-const: senão vira 1313

    fmt::print("      contar_bits(0b10110110):\n");
    fmt::print("        em compilação (laço)      = {}\n", em_compilacao);
    fmt::print("        em runtime (popcount)     = {}\n", em_runtime);
    fmt::print("      Mesma função, dois algoritmos, um resultado.\n");

    // O uso CORRETO: dentro de um `if` comum, is_constant_evaluated() faz
    // exatamente o esperado — true na compilação, false na execução.
    fmt::print("\n      `std::is_constant_evaluated()` num `if` NORMAL funciona:\n");
    fmt::print("        em compilação = {}   em runtime = {}\n",
               ice_em_compilacao, ice_em_runtime);

    fmt::print("\n      ⚠  As DUAS armadilhas reais:\n");
    fmt::print("         (1) `if constexpr (std::is_constant_evaluated())` é\n");
    fmt::print("             SEMPRE true — a própria condição do `if constexpr`\n");
    fmt::print("             é avaliada em compilação. Medido aqui:\n");
    fmt::print("               em compilação = {}   em runtime = {}  ← deveria ser 2\n",
               ifc_em_compilacao, ifc_em_runtime);
    fmt::print("         (2) inicializar uma variável `const`/`constexpr` local\n");
    fmt::print("             torna aquela expressão \"manifestamente avaliada em\n");
    fmt::print("             constante\", mesmo numa chamada de runtime:\n");
    fmt::print("               const int n = is_constant_evaluated() ? 13 : 17;\n");
    fmt::print("               int       m = is_constant_evaluated() ? 13 : 17;\n");
    fmt::print("             numa chamada de RUNTIME: n={}, m={}\n",
               armadilha_const_local_rt / 100, armadilha_const_local_rt % 100);
    fmt::print("         `if consteval` não tem nenhuma das duas: a sintaxe\n");
    fmt::print("         impede o uso errado.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_garantia();
    demo_propagacao();
    demo_validacao();
    demo_constinit();
    demo_if_consteval();
}

} // namespace demo_consteval
