#pragma once
// =============================================================================
//  tema_claro.hpp — uma família CONCRETA completa
//
//  Repare que as três classes concretas são `final` e vivem num namespace
//  próprio: ninguém fora daqui precisa conhecê-las. O mundo externo só vê
//  `FabricaDeWidgets`.
// =============================================================================
#include "widgets.hpp"

#include <memory>
#include <string>

namespace abstract_factory::claro {

class Botao final : public abstract_factory::Botao {
public:
    explicit Botao(std::string rotulo) : rotulo_(std::move(rotulo)) {}
    std::string desenhar() const override;
    std::string ao_clicar() const override;
private:
    std::string rotulo_;
};

class Campo final : public CampoDeTexto {
public:
    std::string desenhar(const std::string& conteudo) const override;
};

class Caixa final : public CaixaDeSelecao {
public:
    std::string desenhar(bool marcada) const override;
};

class Fabrica final : public FabricaDeWidgets {
public:
    std::unique_ptr<abstract_factory::Botao> criar_botao(std::string r) const override {
        return std::make_unique<Botao>(std::move(r));
    }
    std::unique_ptr<CampoDeTexto> criar_campo() const override {
        return std::make_unique<Campo>();
    }
    std::unique_ptr<CaixaDeSelecao> criar_caixa() const override {
        return std::make_unique<Caixa>();
    }
    std::string nome_do_tema() const override { return "claro"; }
};

}  // namespace abstract_factory::claro
