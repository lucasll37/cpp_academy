#pragma once
// =============================================================================
//  formulario.hpp — o MEDIADOR CONCRETO
//
//  Toda a regra de interação mora aqui. É o único lugar do programa que sabe
//  como os componentes se relacionam.
// =============================================================================
#include "componentes.hpp"
#include "mediador.hpp"

#include <string>
#include <vector>

namespace mediator {

class FormularioDeCadastro final : public Mediador {
public:
    FormularioDeCadastro();

    void notificar(Componente* origem, const std::string& evento) override;

    // Acesso para o demo inspecionar o estado
    CampoDeTexto&    email()  { return email_; }
    CampoDeTexto&    cnpj()   { return cnpj_; }
    CaixaDeSelecao&  termos() { return termos_; }
    CaixaDeSelecao&  empresa(){ return empresa_; }
    Botao&           enviar() { return enviar_; }
    const Rotulo&    status() const { return status_; }

    const std::vector<std::string>& registro() const { return registro_; }

private:
    void revalidar();
    static bool email_valido(const std::string& e);

    CampoDeTexto   email_{"email"};
    CampoDeTexto   cnpj_{"cnpj"};
    CaixaDeSelecao termos_{"termos"};
    CaixaDeSelecao empresa_{"empresa"};
    Botao          enviar_{"enviar"};
    Rotulo         status_{"status"};

    std::vector<std::string> registro_;   // log das notificações, para o demo
};

}  // namespace mediator
