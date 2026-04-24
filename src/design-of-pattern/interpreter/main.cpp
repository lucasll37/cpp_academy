// =============================================================================
//  src/interpreter/main.cpp
//  Demos do padrão Interpreter
// =============================================================================
#include "expression.hpp"
#include "parser.hpp"
#include "program.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::gold) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n",
               title);
}

static void eval(const std::string& expr_str,
                 interpreter::Context& ctx,
                 const std::string& label = "") {
    auto ast    = interpreter::parse(expr_str);
    double result = ast->interpret(ctx);
    std::string name = label.empty() ? ast->to_string() : label;
    fmt::print("  {:30} = {}\n", name, result);
}

// =============================================================================
//  Demo 1 — Construção manual da AST
//
//  Mostra o padrão GoF puro: cada nó da gramática é um objeto.
//  O programador monta a árvore diretamente usando os helpers.
// =============================================================================
void demo_ast_manual() {
    section("1 · AST manual — nós como objetos");

    interpreter::Context ctx;

    fmt::print("\nExpressão: (2 + 3) * 4\n");
    //
    //       MulExpr
    //      /       \
    //  AddExpr    NumberExpr(4)
    //  /     \
    // 2       3
    //
    auto expr = interpreter::mul(
        interpreter::add(interpreter::num(2), interpreter::num(3)),
        interpreter::num(4)
    );

    fmt::print("  AST: {}\n", expr->to_string());
    fmt::print("  Resultado: {}\n\n", expr->interpret(ctx));

    fmt::print("Expressão: -(x + 1) * 2, com x=5\n");
    ctx.set("x", 5.0);

    auto expr2 = interpreter::mul(
        interpreter::neg(
            interpreter::add(interpreter::var("x"), interpreter::num(1))
        ),
        interpreter::num(2)
    );

    fmt::print("  AST: {}\n", expr2->to_string());
    fmt::print("  Resultado: {}\n", expr2->interpret(ctx));
}

// =============================================================================
//  Demo 2 — Parser: string → AST automaticamente
//
//  O Parser recursivo descendente transforma texto em AST.
//  Compara com src/parser/ onde Flex/Bison fazem isso com ferramentas externas.
// =============================================================================
void demo_parser() {
    section("2 · Parser recursivo descendente — string → AST");

    interpreter::Context ctx;

    fmt::print("\nExpressões simples:\n");
    eval("2 + 3 * 4",        ctx); // = 14 (precedência correta)
    eval("(2 + 3) * 4",      ctx); // = 20
    eval("10 / (2 + 3)",     ctx); // = 2
    eval("-5 + 3",           ctx); // = -2
    eval("2 + 3 - 1 + 4",    ctx); // = 8 (left-associative)
    eval("100 / 5 / 4",      ctx); // = 5  (100/5=20, 20/4=5)

    fmt::print("\nCom variáveis no contexto:\n");
    ctx.set("pi", 3.14159);
    ctx.set("r",  10.0);
    eval("pi * r * r", ctx, "área do círculo");   // ≈ 314.159
    eval("2 * pi * r", ctx, "circunferência");     // ≈ 62.83

    fmt::print("\nAninhamento profundo:\n");
    eval("((1 + 2) * (3 + 4)) / (2 + 5)", ctx); // = 3
}

// =============================================================================
//  Demo 3 — Reutilização de AST: mesmo nó, contextos diferentes
//
//  Uma das vantagens do Interpreter: a AST é um objeto reutilizável.
//  Compile uma vez, avalie com contextos diferentes (lote).
// =============================================================================
void demo_reuso_ast() {
    section("3 · Reutilização da AST com contextos diferentes");

    // Fórmula da equação do segundo grau: (-b + sqrt(discriminante)) / (2*a)
    // Simplificado: (b * b - 4 * a * c) como discriminante
    auto discriminante = interpreter::parse("b * b - 4 * a * c");

    fmt::print("\nDiscriminante = b² - 4ac, para diferentes (a,b,c):\n");
    fmt::print("  {:<20} {}\n", "Equação", "Discriminante");
    fmt::print("  {:-<35}\n", "");

    struct Case { double a, b, c; std::string label; };
    std::vector<Case> cases = {
        {1, -5,  6,  "x² - 5x + 6"},   // (x-2)(x-3), Δ=1
        {1,  0, -1,  "x² - 1"},         // (x-1)(x+1), Δ=4
        {1,  2,  5,  "x² + 2x + 5"},   // sem raízes reais, Δ=-16
        {2, -3, -2,  "2x² - 3x - 2"},  // Δ=25
    };

    for (const auto& c : cases) {
        interpreter::Context ctx;
        ctx.set("a", c.a);
        ctx.set("b", c.b);
        ctx.set("c", c.c);
        double delta = discriminante->interpret(ctx);
        fmt::print("  {:<20} {} {}\n",
                   c.label, delta,
                   delta < 0 ? "(sem raízes reais)" :
                   delta == 0 ? "(raiz dupla)" : "");
    }
}

// =============================================================================
//  Demo 4 — Program: mini-linguagem com statements
//
//  Mostra que o Interpreter escala para linguagens completas.
//  Compare com um script Python equivalente comentado abaixo.
// =============================================================================
void demo_program() {
    section("4 · Program — mini-linguagem com statements");

    const std::string source = R"(
# Cálculo de juros compostos: M = P * (1 + r)^n
# (sem exponenciação nativa — usando multiplicações)
P = 1000
r = 0.1
n1 = P * 1.1
n2 = n1 * 1.1
n3 = n2 * 1.1
n4 = n3 * 1.1
n5 = n4 * 1.1

print n1
print n2
print n5

# Taxa acumulada (relativa ao principal)
ganho = n5 - P
print ganho
)";

    fmt::print("\nFonte do programa:\n");
    // Imprime o fonte sem linhas vazias ou só comentários
    std::istringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find_first_not_of(" \t\r\n") != std::string::npos)
            fmt::print("  {}\n", line);
    }

    fmt::print("\nExecutando:\n");
    auto program = interpreter::ProgramParser::parse(source);
    interpreter::Context ctx;
    program.run(ctx);
}

// =============================================================================
//  Demo 5 — Comparação com src/parser/ (Flex/Bison)
// =============================================================================
void demo_comparacao() {
    section("5 · Interpreter GoF vs Flex/Bison — quando usar cada um");

    fmt::print(R"(
  Interpreter GoF (este subprojeto):
    ✓ Zero dependências externas — só C++ puro
    ✓ AST é um objeto reutilizável (avalie N vezes com contextos diferentes)
    ✓ Fácil de testar unitariamente (cada nó é uma classe)
    ✓ Fácil de estender (novo operador = nova classe)
    ✓ Ideal para DSLs embutidas no código (config, regras de negócio)
    ✗ Parser manual é trabalhoso para gramáticas grandes
    ✗ Sem recuperação de erros sofisticada
    ✗ Performance menor que parser gerado para gramáticas grandes

  Flex/Bison (src/parser/):
    ✓ Ideal para gramáticas complexas (linguagens de programação reais)
    ✓ Recuperação de erros robusta
    ✓ Performance máxima (parser LALR(1) gerado)
    ✓ Ferramentas maduras com décadas de uso
    ✗ Requer ferramentas externas (flex, bison)
    ✗ AST precisa ser implementada separadamente
    ✗ Curva de aprendizado mais íngreme

  Regra prática:
    • Gramática < 20 regras, embutida no C++ → Interpreter GoF
    • Linguagem completa, arquivo de entrada, erros amigáveis → Flex/Bison
)");
}

int main() {
    demo_ast_manual();
    demo_parser();
    demo_reuso_ast();
    demo_program();
    demo_comparacao();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos do Interpreter concluídos!\n\n");
}
