#pragma once
// =============================================================================
//  demo_actors.hpp — SRP pelo ângulo dos "atores": quem pede a mudança?
// =============================================================================
//
//  A definição precisa de Uncle Bob fala em "ator" (actor), não em
//  "responsabilidade" genérica:
//
//    "A module should be responsible to one, and only one, actor."
//         — Robert C. Martin, "Clean Architecture" (2017)
//
//  "Ator" = grupo de stakeholders que pode exigir a mesma mudança.
//  Exemplos de atores distintos em um sistema corporativo:
//    • COO (operations)  → regras de negócio / cálculos
//    • CFO (finance)     → relatórios financeiros
//    • CTO (technology)  → banco de dados / persistência
//
//  Este demo usa o exemplo clássico do livro "Clean Architecture":
//  a classe Employee que viola SRP porque serve a três atores diferentes.
//
// =============================================================================

#include <fmt/core.h>
#include <string>
#include <vector>
#include <cmath>

namespace demo_actors {

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO CLÁSSICA — "Employee" do livro Clean Architecture (Cap. 7)
//
//  Três métodos, três atores diferentes:
//    calculatePay()   → demandado pelo COO / RH (regras de horas e pagamento)
//    reportHours()    → demandado pelo CFO (auditoria financeira)
//    save()           → demandado pelo CTO / DBA (esquema do banco)
//
//  Se o método privado regularHours() for compartilhado entre
//  calculatePay() e reportHours(), uma mudança pedida pelo COO
//  pode quebrar acidentalmente o relatório do CFO!
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

struct Employee {
    std::string name;
    double      regular_hours;
    double      overtime_hours;
    double      hourly_rate;
    bool        is_exempt;         // funcionário isento de hora extra

    // Compartilhado internamente — ponto de acoplamento oculto!
    double total_hours() const {
        return regular_hours + overtime_hours;
    }

    // Ator: COO/RH — calcula pagamento com regras específicas de RH
    double calculate_pay() const {
        double pay = regular_hours * hourly_rate;
        if (!is_exempt)
            pay += overtime_hours * hourly_rate * 1.5; // hora extra = 150%
        return pay;
    }

    // Ator: CFO — usa total_hours() para relatório de auditoria
    // PROBLEMA: se COO pedir para mudar total_hours() (ex: excluir horas de
    // treinamento), o relatório do CFO muda sem que ele tenha pedido!
    std::string report_hours() const {
        return fmt::format("  {} — horas totais: {:.1f}h (regular: {:.1f}h + extra: {:.1f}h)",
                           name, total_hours(), regular_hours, overtime_hours);
    }

    // Ator: CTO/DBA — persiste no banco de dados
    void save() const {
        fmt::print("  [DB] INSERT INTO employees(name, reg_h, ot_h, rate) "
                   "VALUES ('{}', {}, {}, {})\n",
                   name, regular_hours, overtime_hours, hourly_rate);
    }
};

void demo() {
    fmt::print("\n  ❌ Employee serve a 3 atores (COO, CFO, CTO):\n");
    Employee e{"Alice", 160.0, 20.0, 50.0, false};

    fmt::print("  calculate_pay()  → R$ {:.2f}  (ator: COO/RH)\n",
               e.calculate_pay());
    fmt::print("  {}\n  (ator: CFO)\n", e.report_hours());
    e.save();

    fmt::print("\n  Risco: mudar total_hours() para o COO afeta o CFO!\n");
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — três fachadas, cada uma responsável perante um ator
//
//  Solução do livro: separar em facades/classes distintas.
//  O dado bruto (EmployeeData) é compartilhado mas sem comportamento.
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

// Dado puro — sem comportamento, sem lógica de negócio
struct EmployeeData {
    std::string name;
    double      regular_hours;
    double      overtime_hours;
    double      hourly_rate;
    bool        is_exempt;
};

// ── Ator: COO/RH — única razão para mudar: regras de pagamento ───────────────
class PayCalculator {
public:
    double calculate(const EmployeeData& e) const {
        double pay = e.regular_hours * e.hourly_rate;
        if (!e.is_exempt)
            pay += e.overtime_hours * e.hourly_rate * 1.5;
        return pay;
    }

    // COO pediu: bônus para funcionários com mais de 200h no mês
    double calculate_with_bonus(const EmployeeData& e) const {
        double base = calculate(e);
        if (total_productive_hours(e) > 200.0)
            base *= 1.1; // 10% de bônus
        return base;
    }

private:
    // Método privado próprio — não compartilhado com outros atores!
    double total_productive_hours(const EmployeeData& e) const {
        return e.regular_hours + e.overtime_hours;
    }
};

// ── Ator: CFO — única razão para mudar: formato ou conteúdo do relatório ─────
class HourReporter {
public:
    std::string report(const EmployeeData& e) const {
        // CFO usa total de horas de forma diferente do COO:
        // aqui inclui horas extras completas para auditoria
        double auditable_hours = e.regular_hours + e.overtime_hours;
        return fmt::format(
            "  {:20s} | regular: {:6.1f}h | extra: {:5.1f}h | total: {:6.1f}h",
            e.name, e.regular_hours, e.overtime_hours, auditable_hours);
    }
};

// ── Ator: CTO — única razão para mudar: esquema do banco / ORM ───────────────
class EmployeeRepository {
public:
    void save(const EmployeeData& e) const {
        fmt::print("  [DB] INSERT INTO employees(name, reg_h, ot_h, rate, exempt) "
                   "VALUES ('{}', {:.1f}, {:.1f}, {:.2f}, {})\n",
                   e.name, e.regular_hours, e.overtime_hours,
                   e.hourly_rate, e.is_exempt ? 1 : 0);
    }

    // CTO pode adicionar save_audit_log() sem afetar PayCalculator ou HourReporter
    void save_audit_log(const EmployeeData& e) const {
        fmt::print("  [AUDIT] {} — dados persistidos em {}\n",
                   e.name, "2024-01-15T10:30:00Z");
    }
};

// ── Fachada opcional: Employee agrega as três, mas delega — não implementa ───
// Se preferir manter a interface original para os clientes:
class Employee {
public:
    explicit Employee(EmployeeData data)
        : data_(std::move(data)) {}

    double      pay()                  const { return calc_.calculate(data_); }
    std::string report()               const { return reporter_.report(data_); }
    void        save()                 const { repo_.save(data_); }

private:
    EmployeeData       data_;
    PayCalculator      calc_;
    HourReporter       reporter_;
    EmployeeRepository repo_;
};

void demo() {
    fmt::print("\n  ✅ Separação por ator — cada classe tem um dono:\n");

    EmployeeData data{"Alice", 160.0, 20.0, 50.0, false};

    PayCalculator      calc;
    HourReporter       reporter;
    EmployeeRepository repo;

    fmt::print("  COO → PayCalculator::calculate()   = R$ {:.2f}\n",
               calc.calculate(data));
    fmt::print("  COO → com bônus (>200h)            = R$ {:.2f}\n",
               calc.calculate_with_bonus(data));
    fmt::print("  CFO → HourReporter::report():\n");
    fmt::print("  {}\n", reporter.report(data));
    fmt::print("  CTO → EmployeeRepository::save():\n");
    repo.save(data);
    repo.save_audit_log(data);

    fmt::print("\n  Mudança no cálculo de horas do COO NÃO afeta\n");
    fmt::print("  o relatório do CFO — total_productive_hours() é privado!\n");
}

} // namespace good

inline void run() {
    fmt::print("┌─ Demo 4: SRP pelo ângulo dos Atores (Clean Architecture) ┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_actors
