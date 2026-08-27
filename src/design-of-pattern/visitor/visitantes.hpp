#pragma once
// =============================================================================
//  visitantes.hpp — TRÊS operações, sem tocar na hierarquia
//
//  Este é o ponto do padrão: cada arquivo destes é uma operação NOVA, e
//  `ast.hpp` não muda uma linha.
// =============================================================================
#include "ast.hpp"

#include <map>
#include <string>

namespace visitor {

// ── 1. Avaliar ───────────────────────────────────────────────────────────────
class Avaliador final : public Visitante {
public:
    explicit Avaliador(std::map<std::string, double> vars = {})
        : vars_(std::move(vars)) {}

    void visitar(const Numero& n) override;
    void visitar(const Variavel& v) override;
    void visitar(const Soma& s) override;
    void visitar(const Multiplicacao& m) override;
    void visitar(const Negacao& n) override;

    double resultado() const { return resultado_; }

private:
    // O visitante acumula estado — é ele que carrega o "retorno", já que
    // `visitar` devolve void. É a limitação clássica da assinatura do GoF.
    double                        resultado_ = 0;
    std::map<std::string, double> vars_;
};

// ── 2. Imprimir ──────────────────────────────────────────────────────────────
class Impressor final : public Visitante {
public:
    void visitar(const Numero& n) override;
    void visitar(const Variavel& v) override;
    void visitar(const Soma& s) override;
    void visitar(const Multiplicacao& m) override;
    void visitar(const Negacao& n) override;

    const std::string& texto() const { return texto_; }

private:
    std::string texto_;
};

// ── 3. Contar nós por tipo ───────────────────────────────────────────────────
class Contador final : public Visitante {
public:
    void visitar(const Numero&) override       { ++numeros_; }
    void visitar(const Variavel&) override     { ++variaveis_; }
    void visitar(const Soma& s) override;
    void visitar(const Multiplicacao& m) override;
    void visitar(const Negacao& n) override;

    int numeros() const    { return numeros_; }
    int variaveis() const  { return variaveis_; }
    int operacoes() const  { return operacoes_; }
    int profundidade() const { return max_prof_; }

private:
    int numeros_ = 0, variaveis_ = 0, operacoes_ = 0;
    int prof_ = 0, max_prof_ = 0;

    void entrar() { ++prof_; if (prof_ > max_prof_) max_prof_ = prof_; }
    void sair()   { --prof_; }
};

}  // namespace visitor
