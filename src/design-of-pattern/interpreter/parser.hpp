// =============================================================================
//  src/interpreter/parser.hpp
//
//  Parser recursivo descendente — constrói a AST
//  ──────────────────────────────────────────────
//  Enquanto o src/parser/ usa Flex/Bison para GERAR um parser,
//  este arquivo implementa o parser MANUALMENTE em C++ puro.
//
//  Técnica: Recursive Descent Parser
//    • Cada regra da gramática vira uma função
//    • Funções se chamam recursivamente conforme a gramática
//    • Resultado: árvore de Expression (AST)
//
//  Gramática implementada (em BNF):
//    expr    := term   { ('+' | '-') term   }
//    term    := factor { ('*' | '/') factor }
//    factor  := NUMBER | VARIABLE | '(' expr ')' | '-' factor
//
//  A precedência sai naturalmente da estrutura:
//    expr chama term → term é avaliado antes de +/-
//    term chama factor → factor é avaliado antes de *\//
// =============================================================================
#pragma once
#include "expression.hpp"
#include <string>
#include <string_view>
#include <stdexcept>
#include <cctype>

namespace interpreter {

class Parser {
public:
    explicit Parser(std::string input)
        : input_(std::move(input)), pos_(0) {}

    // parse() → ponto de entrada, retorna a AST completa
    ExprPtr parse() {
        skip_whitespace();
        auto expr = parse_expr();
        skip_whitespace();
        if (pos_ < input_.size())
            throw std::runtime_error(
                fmt::format("caractere inesperado '{}' na posição {}",
                            input_[pos_], pos_));
        return expr;
    }

private:
    std::string input_;
    std::size_t pos_;

    // ── Regras da gramática (uma função por regra) ────────────────────────────

    // expr := term { ('+' | '-') term }
    // Laço porque + e - são left-associative: a+b+c = (a+b)+c
    ExprPtr parse_expr() {
        auto left = parse_term();

        while (pos_ < input_.size()) {
            skip_whitespace();
            if (pos_ >= input_.size()) break;

            char op = input_[pos_];
            if (op != '+' && op != '-') break;

            ++pos_; // consome o operador
            skip_whitespace();
            auto right = parse_term();

            if (op == '+') left = add(std::move(left), std::move(right));
            else            left = sub(std::move(left), std::move(right));
        }

        return left;
    }

    // term := factor { ('*' | '/') factor }
    ExprPtr parse_term() {
        auto left = parse_factor();

        while (pos_ < input_.size()) {
            skip_whitespace();
            if (pos_ >= input_.size()) break;

            char op = input_[pos_];
            if (op != '*' && op != '/') break;

            ++pos_;
            skip_whitespace();
            auto right = parse_factor();

            if (op == '*') left = mul(std::move(left), std::move(right));
            else            left = div(std::move(left), std::move(right));
        }

        return left;
    }

    // factor := NUMBER | VARIABLE | '(' expr ')' | '-' factor
    ExprPtr parse_factor() {
        skip_whitespace();
        if (pos_ >= input_.size())
            throw std::runtime_error("expressão incompleta");

        char c = input_[pos_];

        // Número: dígito ou '.' inicial
        if (std::isdigit(c) || c == '.') {
            return parse_number();
        }

        // Variável: letra ou underscore
        if (std::isalpha(c) || c == '_') {
            return parse_variable();
        }

        // Parênteses: '(' expr ')'
        if (c == '(') {
            ++pos_; // consome '('
            auto inner = parse_expr();
            skip_whitespace();
            if (pos_ >= input_.size() || input_[pos_] != ')')
                throw std::runtime_error("')' esperado");
            ++pos_; // consome ')'
            return inner;
        }

        // Negação unária: '-' factor
        if (c == '-') {
            ++pos_;
            skip_whitespace();
            auto operand = parse_factor();
            return neg(std::move(operand));
        }

        throw std::runtime_error(
            fmt::format("token inesperado '{}' na posição {}", c, pos_));
    }

    // ── Reconhecedores de terminais ───────────────────────────────────────────

    ExprPtr parse_number() {
        std::size_t start = pos_;
        while (pos_ < input_.size() &&
               (std::isdigit(input_[pos_]) || input_[pos_] == '.'))
            ++pos_;

        double value = std::stod(input_.substr(start, pos_ - start));
        return num(value);
    }

    ExprPtr parse_variable() {
        std::size_t start = pos_;
        while (pos_ < input_.size() &&
               (std::isalnum(input_[pos_]) || input_[pos_] == '_'))
            ++pos_;

        return var(input_.substr(start, pos_ - start));
    }

    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(input_[pos_]))
            ++pos_;
    }
};

// Função utilitária: string → AST
inline ExprPtr parse(const std::string& expr) {
    return Parser(expr).parse();
}

} // namespace interpreter
