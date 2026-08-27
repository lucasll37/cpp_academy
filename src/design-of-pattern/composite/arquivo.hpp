#pragma once
// =============================================================================
//  arquivo.hpp — a FOLHA
//
//  Uma folha não tem filhos, e por isso responde às operações de forma trivial.
//  Repare que ela NÃO precisa fingir que tem `adicionar()` — a interface não
//  a obriga a mentir.
// =============================================================================
#include "no.hpp"

#include <string>

namespace composite {

class Arquivo final : public No {
public:
    Arquivo(std::string nome, std::size_t bytes)
        : nome_(std::move(nome)), bytes_(bytes) {}

    std::string nome() const override { return nome_; }
    std::size_t tamanho() const override { return bytes_; }
    std::size_t contar_arquivos() const override { return 1; }
    void        imprimir_em(int nivel) const override;

private:
    std::string nome_;
    std::size_t bytes_;
};

}  // namespace composite
