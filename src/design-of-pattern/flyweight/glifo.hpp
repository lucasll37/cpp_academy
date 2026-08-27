#pragma once
// =============================================================================
//  glifo.hpp — o FLYWEIGHT e a divisão do estado
//
//  A ideia central: separar o estado de um objeto em duas partes.
//
//    INTRÍNSECO  — compartilhável, imutável, independe do contexto
//                  (o desenho da letra 'a' na fonte Arial 12)
//    EXTRÍNSECO  — único por instância, passado como PARÂMETRO
//                  (a posição x,y onde essa letra 'a' aparece)
//
//  Um documento com 100.000 caracteres não precisa de 100.000 objetos de
//  fonte: precisa de ~100 glifos compartilhados e 100.000 posições.
//
//  A economia é medida no main.cpp — e é grande.
// =============================================================================
#include <cstddef>
#include <string>

namespace flyweight {

// ── O estado INTRÍNSECO: pesado, compartilhado, imutável ─────────────────────
class Glifo {
public:
    Glifo(char c, std::string fonte, int tamanho);

    // O estado EXTRÍNSECO (x, y, cor) chega como PARÂMETRO — o glifo não o guarda
    std::string desenhar(int x, int y, const std::string& cor) const;

    char               caractere() const { return caractere_; }
    const std::string& fonte() const     { return fonte_; }
    int                tamanho() const   { return tamanho_; }

    // O peso real: um bitmap de rasterização
    std::size_t bytes() const;

    static int construcoes;   // para o demo contar

private:
    char        caractere_;
    std::string fonte_;
    int         tamanho_;
    std::string bitmap_;      // ← o que torna o objeto CARO
};

}  // namespace flyweight
