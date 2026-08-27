#include "visitantes.hpp"

#include <fmt/core.h>

namespace visitor {

// ── Avaliador ────────────────────────────────────────────────────────────────
void Avaliador::visitar(const Numero& n) { resultado_ = n.valor(); }

void Avaliador::visitar(const Variavel& v) {
    const auto it = vars_.find(v.nome());
    resultado_ = (it != vars_.end()) ? it->second : 0.0;
}

void Avaliador::visitar(const Soma& s) {
    s.esquerda().aceitar(*this);
    const double e = resultado_;          // salva antes de sobrescrever
    s.direita().aceitar(*this);
    resultado_ = e + resultado_;
}

void Avaliador::visitar(const Multiplicacao& m) {
    m.esquerda().aceitar(*this);
    const double e = resultado_;
    m.direita().aceitar(*this);
    resultado_ = e * resultado_;
}

void Avaliador::visitar(const Negacao& n) {
    n.interna().aceitar(*this);
    resultado_ = -resultado_;
}

// ── Impressor ────────────────────────────────────────────────────────────────
void Impressor::visitar(const Numero& n)   { texto_ += fmt::format("{:g}", n.valor()); }
void Impressor::visitar(const Variavel& v) { texto_ += v.nome(); }

void Impressor::visitar(const Soma& s) {
    texto_ += "(";
    s.esquerda().aceitar(*this);
    texto_ += " + ";
    s.direita().aceitar(*this);
    texto_ += ")";
}

void Impressor::visitar(const Multiplicacao& m) {
    texto_ += "(";
    m.esquerda().aceitar(*this);
    texto_ += " * ";
    m.direita().aceitar(*this);
    texto_ += ")";
}

void Impressor::visitar(const Negacao& n) {
    texto_ += "-";
    n.interna().aceitar(*this);
}

// ── Contador ─────────────────────────────────────────────────────────────────
void Contador::visitar(const Soma& s) {
    ++operacoes_; entrar();
    s.esquerda().aceitar(*this);
    s.direita().aceitar(*this);
    sair();
}

void Contador::visitar(const Multiplicacao& m) {
    ++operacoes_; entrar();
    m.esquerda().aceitar(*this);
    m.direita().aceitar(*this);
    sair();
}

void Contador::visitar(const Negacao& n) {
    ++operacoes_; entrar();
    n.interna().aceitar(*this);
    sair();
}

}  // namespace visitor
