#pragma once
// =============================================================================
//  mediador.hpp — o coordenador central
//
//  O Mediator troca uma teia de referências mútuas por uma ESTRELA.
//
//    SEM mediator, N componentes que conversam entre si:
//      cada um conhece os outros → N×(N-1) referências
//      5 componentes = 20 referências
//      acrescentar um = tocar todos os outros
//
//    COM mediator:
//      cada um conhece só o mediador → N referências
//      5 componentes = 5 referências
//      acrescentar um = registrar no mediador
//
//  O trade-off: o mediador CONCENTRA a lógica de interação. Ele pode virar um
//  God Object — é o risco que a seção 5 do main.cpp discute.
// =============================================================================
#include <string>

namespace mediator {

class Componente;

class Mediador {
public:
    virtual ~Mediador() = default;

    // O único método que os componentes conhecem
    virtual void notificar(Componente* origem, const std::string& evento) = 0;
};

// A base dos componentes: cada um conhece SÓ o mediador
class Componente {
public:
    explicit Componente(std::string nome) : nome_(std::move(nome)) {}
    virtual ~Componente() = default;

    void definir_mediador(Mediador* m) { mediador_ = m; }
    const std::string& nome() const { return nome_; }

protected:
    void emitir(const std::string& evento) {
        if (mediador_) mediador_->notificar(this, evento);
    }

    std::string nome_;
    Mediador*   mediador_ = nullptr;   // ← a ÚNICA referência externa
};

}  // namespace mediator
