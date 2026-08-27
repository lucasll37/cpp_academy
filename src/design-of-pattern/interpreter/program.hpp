// =============================================================================
//  src/interpreter/program.hpp
//
//  Extensão da linguagem: statements e variáveis
//  ──────────────────────────────────────────────
//  Mostra como o Interpreter escala para além de expressões simples.
//  Adicionamos:
//    • AssignStatement   — x = expr  (atribui variável no contexto)
//    • PrintStatement    — print expr (imprime resultado)
//    • Program           — sequência de statements
//
//  A linguagem completa fica:
//    program  := { statement }
//    statement := VARIABLE '=' expr
//              | 'print' expr
//    expr     := (como em expression.hpp)
//
//  Isso demonstra como Interpreter GoF escala para mini-linguagens
//  sem precisar de Flex/Bison.
// =============================================================================
#pragma once
#include "expression.hpp"
#include "parser.hpp"
#include <vector>
#include <memory>

namespace interpreter {

// ─────────────────────────────────────────────────────────────────────────────
//  Statement — instrução que não retorna valor, mas tem efeito no contexto
// ─────────────────────────────────────────────────────────────────────────────
struct Statement {
    virtual ~Statement() = default;
    virtual void execute(Context& ctx) const = 0;
    virtual std::string to_string() const = 0;
};

using StmtPtr = std::unique_ptr<Statement>;

// AssignStatement — x = expr
// Avalia expr e armazena o resultado no contexto com o nome da variável
struct AssignStatement : Statement {
    std::string name_;
    ExprPtr     value_;

    AssignStatement(std::string name, ExprPtr val)
        : name_(std::move(name)), value_(std::move(val)) {}

    void execute(Context& ctx) const override {
        double result = value_->interpret(ctx);
        ctx.set(name_, result);
    }

    std::string to_string() const override {
        return fmt::format("{} = {}", name_, value_->to_string());
    }
};

// PrintStatement — print expr
// Avalia expr e imprime o resultado
struct PrintStatement : Statement {
    ExprPtr expr_;
    std::string label_; // rótulo opcional para o output

    explicit PrintStatement(ExprPtr e, std::string label = "")
        : expr_(std::move(e)), label_(std::move(label)) {}

    void execute(Context& ctx) const override {
        double result = expr_->interpret(ctx);
        if (label_.empty())
            fmt::print("  → {} = {}\n", expr_->to_string(), result);
        else
            fmt::print("  → {} = {}\n", label_, result);
    }

    std::string to_string() const override {
        return fmt::format("print {}", expr_->to_string());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Program — sequência de statements
//  Interpreta cada um em ordem, compartilhando o mesmo Context
// ─────────────────────────────────────────────────────────────────────────────
struct Program {
    std::vector<StmtPtr> statements_;

    Program& add(StmtPtr stmt) {
        statements_.push_back(std::move(stmt));
        return *this;
    }

    void run(Context& ctx) const {
        for (const auto& stmt : statements_)
            stmt->execute(ctx);
    }

    void print() const {
        fmt::print("  Programa ({} statements):\n", statements_.size());
        for (const auto& s : statements_)
            fmt::print("    {}\n", s->to_string());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ProgramParser — parseia texto multi-linha para um Program
//
//  Formato aceito:
//    x = 2 + 3
//    y = x * 4
//    print y - 1
//    print (x + y) / 2
// ─────────────────────────────────────────────────────────────────────────────
class ProgramParser {
public:
    static Program parse(const std::string& source) {
        Program program;
        std::istringstream stream(source);
        std::string line;

        while (std::getline(stream, line)) {
            // Remove comentários (#)
            auto comment = line.find('#');
            if (comment != std::string::npos)
                line = line.substr(0, comment);

            // Remove espaços em branco
            auto start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue; // linha vazia
            line = line.substr(start);
            if (line.empty()) continue;

            // "print expr"
            if (line.rfind("print ", 0) == 0) {
                auto expr_str = line.substr(6);
                program.add(
                    std::make_unique<PrintStatement>(
                        interpreter::parse(expr_str),
                        expr_str));
                continue;
            }

            // "var = expr"
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                auto name = line.substr(0, eq);
                // Remove espaços do nome
                auto end = name.find_last_not_of(" \t");
                if (end != std::string::npos) name = name.substr(0, end + 1);

                auto expr_str = line.substr(eq + 1);
                program.add(
                    std::make_unique<AssignStatement>(
                        name,
                        interpreter::parse(expr_str)));
                continue;
            }

            throw std::runtime_error("statement inválido: " + line);
        }

        return program;
    }

private:
    // Usa istringstream do cabeçalho — precisa do include aqui
    #include <sstream>
};

} // namespace interpreter
