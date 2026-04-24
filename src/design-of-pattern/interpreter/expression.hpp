// =============================================================================
//  src/interpreter/expression.hpp
//
//  Interpreter — cada regra da gramática é uma classe
//  ────────────────────────────────────────────────────
//  Define uma representação para a gramática de uma linguagem e um
//  interpretador que usa essa representação para interpretar sentenças.
//
//  A linguagem aqui é expressões aritméticas com variáveis:
//
//    expr := number
//           | variable
//           | expr + expr
//           | expr - expr
//           | expr * expr
//           | expr / expr
//           | -expr
//
//  Compare com src/parser/ (Flex/Bison):
//    Flex/Bison: gramática em arquivo .l/.y, geração de código, avaliação direta
//    Interpreter GoF: gramática em hierarquia de classes C++, AST explícita,
//                     múltiplas operações via contexto ou visitor
//
//  Quando usar Interpreter GoF vs Flex/Bison:
//    • Interpreter GoF: gramáticas simples, embutidas no código, testáveis
//    • Flex/Bison: gramáticas complexas, suporte a erros, performance
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <vector>
#include <sstream>
#include <cctype>

namespace interpreter {

// ─────────────────────────────────────────────────────────────────────────────
//  Context — o ambiente de execução
//
//  Guarda as variáveis e seus valores durante a interpretação.
//  Todas as expressões recebem o mesmo Context → compartilham estado.
// ─────────────────────────────────────────────────────────────────────────────
class Context {
public:
    void set(const std::string& name, double value) {
        vars_[name] = value;
        fmt::print("    ctx: {} = {}\n", name, value);
    }

    double get(const std::string& name) const {
        auto it = vars_.find(name);
        if (it == vars_.end())
            throw std::runtime_error("variável não definida: " + name);
        return it->second;
    }

    bool has(const std::string& name) const {
        return vars_.count(name) > 0;
    }

    void print_all() const {
        fmt::print("  Variáveis no contexto:\n");
        for (const auto& [k, v] : vars_)
            fmt::print("    {} = {}\n", k, v);
    }

private:
    std::unordered_map<std::string, double> vars_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Expression — interface base
//  Cada regra da gramática é uma subclasse com seu próprio interpret()
// ─────────────────────────────────────────────────────────────────────────────
struct Expression {
    virtual ~Expression() = default;

    // interpret() → avalia a expressão no contexto dado
    virtual double interpret(Context& ctx) const = 0;

    // to_string() → reconstrói a expressão em texto (útil para debug)
    virtual std::string to_string() const = 0;
};

using ExprPtr = std::unique_ptr<Expression>;

// ─────────────────────────────────────────────────────────────────────────────
//  Terminal Expressions — nós folha (sem filhos)
// ─────────────────────────────────────────────────────────────────────────────

// NumberExpr — literal numérico (regra: expr := number)
struct NumberExpr : Expression {
    double value_;
    explicit NumberExpr(double v) : value_(v) {}

    double interpret(Context&) const override {
        return value_; // não precisa do contexto — valor fixo
    }

    std::string to_string() const override {
        // Remove .0 desnecessário para inteiros
        if (value_ == static_cast<int>(value_))
            return fmt::format("{}", static_cast<int>(value_));
        return fmt::format("{}", value_);
    }
};

// VariableExpr — nome de variável (regra: expr := variable)
struct VariableExpr : Expression {
    std::string name_;
    explicit VariableExpr(std::string name) : name_(std::move(name)) {}

    double interpret(Context& ctx) const override {
        return ctx.get(name_); // busca no contexto
    }

    std::string to_string() const override { return name_; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Non-Terminal Expressions — nós internos (com filhos)
//  Cada operador binário é uma classe separada — cada regra da gramática.
// ─────────────────────────────────────────────────────────────────────────────

// AddExpr — regra: expr := expr + expr
struct AddExpr : Expression {
    ExprPtr left_, right_;

    AddExpr(ExprPtr l, ExprPtr r)
        : left_(std::move(l)), right_(std::move(r)) {}

    double interpret(Context& ctx) const override {
        return left_->interpret(ctx) + right_->interpret(ctx);
    }

    std::string to_string() const override {
        return fmt::format("({} + {})", left_->to_string(), right_->to_string());
    }
};

// SubExpr — regra: expr := expr - expr
struct SubExpr : Expression {
    ExprPtr left_, right_;

    SubExpr(ExprPtr l, ExprPtr r)
        : left_(std::move(l)), right_(std::move(r)) {}

    double interpret(Context& ctx) const override {
        return left_->interpret(ctx) - right_->interpret(ctx);
    }

    std::string to_string() const override {
        return fmt::format("({} - {})", left_->to_string(), right_->to_string());
    }
};

// MulExpr — regra: expr := expr * expr
struct MulExpr : Expression {
    ExprPtr left_, right_;

    MulExpr(ExprPtr l, ExprPtr r)
        : left_(std::move(l)), right_(std::move(r)) {}

    double interpret(Context& ctx) const override {
        return left_->interpret(ctx) * right_->interpret(ctx);
    }

    std::string to_string() const override {
        return fmt::format("({} * {})", left_->to_string(), right_->to_string());
    }
};

// DivExpr — regra: expr := expr / expr
struct DivExpr : Expression {
    ExprPtr left_, right_;

    DivExpr(ExprPtr l, ExprPtr r)
        : left_(std::move(l)), right_(std::move(r)) {}

    double interpret(Context& ctx) const override {
        double divisor = right_->interpret(ctx);
        if (divisor == 0.0)
            throw std::runtime_error("divisão por zero em: " + to_string());
        return left_->interpret(ctx) / divisor;
    }

    std::string to_string() const override {
        return fmt::format("({} / {})", left_->to_string(), right_->to_string());
    }
};

// NegateExpr — regra: expr := -expr  (unário)
struct NegateExpr : Expression {
    ExprPtr operand_;

    explicit NegateExpr(ExprPtr op) : operand_(std::move(op)) {}

    double interpret(Context& ctx) const override {
        return -operand_->interpret(ctx);
    }

    std::string to_string() const override {
        return fmt::format("(-{})", operand_->to_string());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers para construir a AST com sintaxe fluente
//  (alternativa aos make_unique verbosos)
// ─────────────────────────────────────────────────────────────────────────────
inline ExprPtr num(double v)      { return std::make_unique<NumberExpr>(v); }
inline ExprPtr var(std::string n) { return std::make_unique<VariableExpr>(std::move(n)); }

inline ExprPtr add(ExprPtr l, ExprPtr r) { return std::make_unique<AddExpr>(std::move(l), std::move(r)); }
inline ExprPtr sub(ExprPtr l, ExprPtr r) { return std::make_unique<SubExpr>(std::move(l), std::move(r)); }
inline ExprPtr mul(ExprPtr l, ExprPtr r) { return std::make_unique<MulExpr>(std::move(l), std::move(r)); }
inline ExprPtr div(ExprPtr l, ExprPtr r) { return std::make_unique<DivExpr>(std::move(l), std::move(r)); }
inline ExprPtr neg(ExprPtr op)           { return std::make_unique<NegateExpr>(std::move(op)); }

} // namespace interpreter
