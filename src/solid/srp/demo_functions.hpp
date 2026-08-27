#pragma once
// =============================================================================
//  demo_functions.hpp — SRP não se aplica só a classes: funções também
// =============================================================================
//
//  SRP é muitas vezes ensinado apenas como "cada classe = uma responsabilidade".
//  Mas o princípio é mais geral: aplica-se a qualquer unidade de código.
//
//  Uma função que:
//    • Busca dados + transforma + persiste        → 3 responsabilidades
//    • Valida entrada + executa lógica            → 2 responsabilidades
//    • Imprime E retorna resultado                → 2 responsabilidades
//
//  Regra prática: se você precisar de "E" ou "OU" para descrever o
//  que uma função faz, ela provavelmente viola o SRP.
//
//  Este demo mostra funções longas sendo decompostas em funções
//  pequenas e reutilizáveis com responsabilidade única.
//
// =============================================================================

#include <fmt/core.h>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cctype>
#include <sstream>
#include <optional>

namespace demo_functions {

// ─────────────────────────────────────────────────────────────────────────────
//  Cenário: processar e exibir um relatório de notas de alunos
// ─────────────────────────────────────────────────────────────────────────────

struct Student {
    std::string name;
    std::vector<double> grades;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO — uma função que faz tudo de uma vez
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

// Esta função:
//   1. Valida a entrada
//   2. Calcula a média
//   3. Determina a aprovação
//   4. Formata a saída
//   5. Imprime no console
//
// Teste unitário impossível sem capturar stdout.
// Reutilização impossível: precisa da impressão mesmo quando não quer.
void process_and_print_student(const Student& s) {
    // validação misturada com lógica
    if (s.grades.empty()) {
        fmt::print("  [ERRO] {} não tem notas\n", s.name);
        return;
    }

    // cálculo misturado com formatação
    double sum = 0;
    for (double g : s.grades) sum += g;
    double avg = sum / static_cast<double>(s.grades.size());

    // decisão misturada com impressão
    bool passed = avg >= 6.0;
    fmt::print("  {:20s} | média: {:5.2f} | {}\n",
               s.name, avg, passed ? "APROVADO" : "REPROVADO");
}

void demo() {
    fmt::print("\n  ❌ Uma função faz validação + cálculo + decisão + I/O:\n");
    process_and_print_student({"Alice", {8.0, 7.5, 9.0}});
    process_and_print_student({"Bob",   {4.0, 5.0, 3.5}});
    process_and_print_student({"Carol", {}});
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — funções pequenas, cada uma com uma responsabilidade
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

// Responsabilidade única: calcular a média das notas
double average(const std::vector<double>& grades) {
    if (grades.empty()) return 0.0;
    return std::accumulate(grades.begin(), grades.end(), 0.0)
           / static_cast<double>(grades.size());
}

// Responsabilidade única: determinar aprovação com base na média
bool is_passing(double avg, double threshold = 6.0) {
    return avg >= threshold;
}

// Responsabilidade única: validar se o aluno tem dados suficientes
std::optional<std::string> validate(const Student& s) {
    if (s.name.empty())    return "nome não pode ser vazio";
    if (s.grades.empty())  return "aluno sem notas";
    return std::nullopt;  // válido
}

// Responsabilidade única: formatar uma linha do relatório (retorna string)
std::string format_row(const Student& s, double avg, bool passed) {
    return fmt::format("  {:20s} | média: {:5.2f} | {}",
                       s.name, avg, passed ? "APROVADO" : "REPROVADO");
}

// Responsabilidade única: imprimir o relatório completo
void print_report(const std::vector<Student>& students) {
    fmt::print("  {:20s} | {:>12s} | {}\n", "Nome", "Resultado", "Status");
    fmt::print("  {}\n", std::string(50, '-'));

    for (const auto& s : students) {
        if (auto err = validate(s)) {
            fmt::print("  [SKIP] {}: {}\n", s.name, *err);
            continue;
        }

        double avg  = average(s.grades);        // calcula
        bool passed = is_passing(avg);           // decide
        fmt::print("{}\n", format_row(s, avg, passed)); // formata + imprime
    }
}

// Benefício: podemos testar cada função de forma isolada
void demo_unit_tests_inline() {
    fmt::print("\n  Cada função é testável independentemente:\n");

    // Testa average sem precisar de Student nem I/O
    fmt::print("    average({{8, 7, 9}})     = {:.2f} (esperado 8.00)\n",
               average({8.0, 7.0, 9.0}));

    // Testa is_passing sem precisar calcular average
    fmt::print("    is_passing(5.9, 6.0)   = {} (esperado false)\n",
               is_passing(5.9) ? "true" : "false");
    fmt::print("    is_passing(6.0, 6.0)   = {} (esperado true)\n",
               is_passing(6.0) ? "true" : "false");

    // Testa validate sem precisar de notas reais
    fmt::print("    validate({{nome,{{}}}}  = '{}'\n",
               validate({"Bob", {}}).value_or("válido"));
    fmt::print("    validate({{Alice,{{8}}}}) = '{}'\n",
               validate({"Alice", {8.0}}).value_or("válido"));

    // Testa format_row sem precisar de I/O
    std::string row = format_row({"Alice", {8.0}}, 8.0, true);
    fmt::print("    format_row: '{}'\n", row);
}

void demo() {
    fmt::print("\n  ✅ Funções com responsabilidade única:\n");

    std::vector<Student> students = {
        {"Alice",   {8.0, 7.5, 9.0}},
        {"Bob",     {4.0, 5.0, 3.5}},
        {"Carol",   {}},
        {"David",   {6.0, 6.0, 6.0}},
    };

    print_report(students);
    demo_unit_tests_inline();
}

} // namespace good

// ─────────────────────────────────────────────────────────────────────────────
//  Heurística: "Command-Query Separation" (CQS)
//  Um complemento natural ao SRP para funções:
//    • Command: muda estado, retorna void
//    • Query:   lê estado, retorna valor — sem efeito colateral
//  Nunca misture os dois na mesma função.
// ─────────────────────────────────────────────────────────────────────────────
namespace cqs_example {

class Counter {
public:
    // ❌ Anti-padrão: retorna o valor E incrementa ao mesmo tempo
    int increment_and_get() {
        return ++value_;   // efeito colateral + query = confusão
    }

    // ✅ CQS: comando e query separados
    void increment()  { ++value_; }   // Command: muda estado
    int  value() const { return value_; } // Query: lê estado

private:
    int value_ = 0;
};

void demo() {
    fmt::print("\n  CQS — Command-Query Separation (complemento ao SRP):\n");

    Counter c;
    c.increment();
    c.increment();
    fmt::print("  counter = {} (após 2 incrementos)\n", c.value());
    fmt::print("  increment() → Command (void)\n");
    fmt::print("  value()     → Query  (const, sem efeito colateral)\n");
}

} // namespace cqs_example

inline void run() {
    fmt::print("┌─ Demo 3: SRP em Funções e CQS ──────────────────────────┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    cqs_example::demo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_functions
