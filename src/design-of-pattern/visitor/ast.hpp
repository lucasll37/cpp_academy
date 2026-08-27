#pragma once
// =============================================================================
//  ast.hpp — a hierarquia e o mecanismo de DESPACHO DUPLO
//
//  O Visitor existe para resolver um problema específico: acrescentar
//  OPERAÇÕES a uma hierarquia sem modificar as classes dela.
//
//  Sem Visitor, cada operação nova (avaliar, imprimir, otimizar, derivar) vira
//  um método virtual em TODAS as classes — e a hierarquia inteira recompila.
//
//  O mecanismo é o DESPACHO DUPLO, e ele é a parte que confunde:
//
//    1. o cliente chama `no->aceitar(visitante)`
//    2. o despacho virtual descobre o tipo do NÓ  → entra em `Soma::aceitar`
//    3. lá dentro, `v.visitar(*this)` — e agora `*this` é `Soma&`, um tipo
//       CONCRETO, então a resolução de overload escolhe `visitar(Soma&)`
//
//  Duas resoluções em sequência: uma virtual (o nó), uma por overload (a
//  operação). É daí que vem "duplo".
// =============================================================================
#include <memory>
#include <string>
#include <vector>

namespace visitor {

// Declarações antecipadas — o Visitante precisa conhecer todos os nós
class Numero;
class Soma;
class Multiplicacao;
class Negacao;
class Variavel;

// ── A interface do VISITANTE: um método por tipo concreto ────────────────────
class Visitante {
public:
    virtual ~Visitante() = default;

    virtual void visitar(const Numero&) = 0;
    virtual void visitar(const Soma&) = 0;
    virtual void visitar(const Multiplicacao&) = 0;
    virtual void visitar(const Negacao&) = 0;
    virtual void visitar(const Variavel&) = 0;
};

// ── A hierarquia de nós ──────────────────────────────────────────────────────
class Expressao {
public:
    virtual ~Expressao() = default;

    // O ÚNICO método virtual que a hierarquia precisa ter para todas as
    // operações futuras. Ele nunca muda.
    virtual void aceitar(Visitante& v) const = 0;
};

using Ptr = std::unique_ptr<Expressao>;

class Numero final : public Expressao {
public:
    explicit Numero(double v) : valor_(v) {}
    void aceitar(Visitante& v) const override { v.visitar(*this); }
    double valor() const { return valor_; }
private:
    double valor_;
};

class Variavel final : public Expressao {
public:
    explicit Variavel(std::string nome) : nome_(std::move(nome)) {}
    void aceitar(Visitante& v) const override { v.visitar(*this); }
    const std::string& nome() const { return nome_; }
private:
    std::string nome_;
};

class Soma final : public Expressao {
public:
    Soma(Ptr e, Ptr d) : esq_(std::move(e)), dir_(std::move(d)) {}
    void aceitar(Visitante& v) const override { v.visitar(*this); }
    const Expressao& esquerda() const { return *esq_; }
    const Expressao& direita() const  { return *dir_; }
private:
    Ptr esq_, dir_;
};

class Multiplicacao final : public Expressao {
public:
    Multiplicacao(Ptr e, Ptr d) : esq_(std::move(e)), dir_(std::move(d)) {}
    void aceitar(Visitante& v) const override { v.visitar(*this); }
    const Expressao& esquerda() const { return *esq_; }
    const Expressao& direita() const  { return *dir_; }
private:
    Ptr esq_, dir_;
};

class Negacao final : public Expressao {
public:
    explicit Negacao(Ptr e) : interna_(std::move(e)) {}
    void aceitar(Visitante& v) const override { v.visitar(*this); }
    const Expressao& interna() const { return *interna_; }
private:
    Ptr interna_;
};

// Atalhos de construção
inline Ptr num(double v)            { return std::make_unique<Numero>(v); }
inline Ptr var(std::string n)       { return std::make_unique<Variavel>(std::move(n)); }
inline Ptr soma(Ptr a, Ptr b)       { return std::make_unique<Soma>(std::move(a), std::move(b)); }
inline Ptr mult(Ptr a, Ptr b)       { return std::make_unique<Multiplicacao>(std::move(a), std::move(b)); }
inline Ptr neg(Ptr a)               { return std::make_unique<Negacao>(std::move(a)); }

}  // namespace visitor
