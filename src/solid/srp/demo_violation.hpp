#pragma once
// =============================================================================
//  demo_violation.hpp — SRP: violação vs. conformidade
// =============================================================================
//
//  O Princípio da Responsabilidade Única (SRP) é o primeiro dos cinco
//  princípios SOLID, formulado por Robert C. Martin ("Uncle Bob"):
//
//    "A class should have only one reason to change."
//
//  "Razão para mudar" ≡ "responsabilidade" ≡ "ator que demanda a mudança".
//  Se duas equipes diferentes podem exigir mudanças em uma mesma classe,
//  ela tem duas responsabilidades e viola o SRP.
//
//  Este demo apresenta o anti-padrão mais comum ("God Class") lado a lado
//  com a solução SRP-conformante.
//
// =============================================================================

#include <fmt/core.h>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

namespace demo_violation {

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO — ReportManager faz tudo:
//     1. Agrega dados de funcionários        (domínio de negócio)
//     2. Formata o relatório como texto      (apresentação)
//     3. Persiste o arquivo no disco         (infraestrutura)
//
//  Quem pode pedir mudança?
//    • RH          → altera lógica de cálculo de salário médio
//    • UX/Design   → altera formato do relatório
//    • DevOps/Ops  → altera path ou formato de arquivo de saída
//
//  Três razões → três responsabilidades → violação do SRP.
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

struct Employee {
    std::string name;
    double      salary;
    std::string department;
};

class ReportManager {
public:
    // Responsabilidade 1: lógica de negócio
    void add_employee(const Employee& e) {
        employees_.push_back(e);
    }

    double average_salary() const {
        if (employees_.empty()) return 0.0;
        double total = 0;
        for (const auto& e : employees_) total += e.salary;
        return total / static_cast<double>(employees_.size());
    }

    // Responsabilidade 2: formatação / apresentação
    std::string format_report() const {
        std::ostringstream oss;
        oss << "=== EMPLOYEE REPORT ===\n";
        for (const auto& e : employees_)
            oss << fmt::format("  {:20s} | {:>10s} | R$ {:>10.2f}\n",
                               e.name, e.department, e.salary);
        oss << fmt::format("  Average salary: R$ {:.2f}\n", average_salary());
        return oss.str();
    }

    // Responsabilidade 3: persistência / I/O
    void save_to_file(const std::string& path) const {
        // Simulado: apenas imprime o que faria
        fmt::print("  [ReportManager] salvando em '{}' (simulado)\n", path);
        // Em produção: abriria arquivo e escreveria format_report()
    }

private:
    std::vector<Employee> employees_;
};

void demo() {
    fmt::print("\n  ❌ Classe GOD — ReportManager com 3 responsabilidades:\n");

    ReportManager mgr;
    mgr.add_employee({"Alice",  8500.0, "Engineering"});
    mgr.add_employee({"Bob",    7200.0, "Engineering"});
    mgr.add_employee({"Carol",  9100.0, "Management"});

    fmt::print("{}", mgr.format_report());
    mgr.save_to_file("/tmp/report.txt");

    fmt::print("\n  Problema: qualquer mudança no formato, na lógica OU no\n");
    fmt::print("  destino do arquivo força recompilação e reteste de TUDO.\n");
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — três classes, cada uma com exatamente uma razão para mudar
//
//  EmployeeRepository  → responsável pelos dados e regras de negócio
//  ReportFormatter     → responsável pela apresentação
//  ReportExporter      → responsável pela persistência / destino
//
//  Agora cada classe pode mudar independentemente:
//    • Trocar formato CSV sem tocar na lógica de salário
//    • Trocar SQLite por Postgres sem tocar no formatter
//    • Adicionar campo bonus sem tocar no exporter
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

struct Employee {
    std::string name;
    double      salary;
    std::string department;
};

// ── Responsabilidade 1: dados e lógica de negócio ────────────────────────────
class EmployeeRepository {
public:
    void add(const Employee& e) { employees_.push_back(e); }

    const std::vector<Employee>& all() const { return employees_; }

    double average_salary() const {
        if (employees_.empty()) return 0.0;
        double total = 0;
        for (const auto& e : employees_) total += e.salary;
        return total / static_cast<double>(employees_.size());
    }

private:
    std::vector<Employee> employees_;
};

// ── Responsabilidade 2: formatação / apresentação ────────────────────────────
class ReportFormatter {
public:
    // Recebe os dados, não os gerencia — SRP!
    std::string format(const EmployeeRepository& repo) const {
        std::ostringstream oss;
        oss << "=== EMPLOYEE REPORT ===\n";
        for (const auto& e : repo.all())
            oss << fmt::format("  {:20s} | {:>10s} | R$ {:>10.2f}\n",
                               e.name, e.department, e.salary);
        oss << fmt::format("  Average salary: R$ {:.2f}\n",
                           repo.average_salary());
        return oss.str();
    }
};

// ── Responsabilidade 3: persistência / destino ───────────────────────────────
class ReportExporter {
public:
    // Recebe o relatório já formatado — não conhece Employee nem formatter
    void save(const std::string& content, const std::string& path) const {
        fmt::print("  [ReportExporter] salvando em '{}' (simulado)\n", path);
        (void)content; // em produção: escreve no arquivo
    }

    void print_stdout(const std::string& content) const {
        fmt::print("{}", content);
    }
};

void demo() {
    fmt::print("\n  ✅ SRP — três classes com uma responsabilidade cada:\n");

    EmployeeRepository repo;
    repo.add({"Alice",  8500.0, "Engineering"});
    repo.add({"Bob",    7200.0, "Engineering"});
    repo.add({"Carol",  9100.0, "Management"});

    ReportFormatter formatter;
    std::string report = formatter.format(repo);

    ReportExporter exporter;
    exporter.print_stdout(report);
    exporter.save(report, "/tmp/report.txt");

    fmt::print("\n  Cada classe tem exatamente uma razão para mudar.\n");
    fmt::print("  Substituir o formatter por um JSON/CSV formatter não\n");
    fmt::print("  afeta Repository nem Exporter.\n");
}

} // namespace good

inline void run() {
    fmt::print("┌─ Demo 1: Violação vs. Conformidade ─────────────────────┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_violation
