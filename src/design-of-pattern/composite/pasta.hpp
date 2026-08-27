#pragma once
// =============================================================================
//  pasta.hpp — o COMPOSTO
//
//  Ele implementa as mesmas operações, mas DELEGANDO aos filhos e agregando o
//  resultado. É a recursão que dá poder ao padrão: uma pasta com pastas dentro
//  responde `tamanho()` sem que ninguém escreva um laço aninhado.
// =============================================================================
#include "no.hpp"

#include <memory>
#include <string>
#include <vector>

namespace composite {

class Pasta final : public No {
public:
    explicit Pasta(std::string nome) : nome_(std::move(nome)) {}

    std::string nome() const override { return nome_; }

    // As três operações agregam recursivamente
    std::size_t tamanho() const override;
    std::size_t contar_arquivos() const override;
    void        imprimir_em(int nivel) const override;

    // As operações de COMPOSIÇÃO vivem só aqui — não na interface `No`
    Pasta& adicionar(std::unique_ptr<No> filho) {
        filhos_.push_back(std::move(filho));
        return *this;                          // permite encadear
    }

    bool        remover(const std::string& nome);
    std::size_t quantidade() const { return filhos_.size(); }

    // Busca recursiva: devolve `nullptr` se não achar
    const No* encontrar(const std::string& nome) const;

private:
    std::string                       nome_;
    std::vector<std::unique_ptr<No>>  filhos_;
};

}  // namespace composite
