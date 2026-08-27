#include "formulario.hpp"

#include <fmt/core.h>

namespace mediator {

FormularioDeCadastro::FormularioDeCadastro() {
    // O mediador conhece todos; nenhum componente conhece outro.
    //
    // ⚠ Um ARRAY, e não uma braced-init-list: `for (Componente* c : {&email_,
    //   &cnpj_, ...})` não compila, porque a dedução de
    //   `std::initializer_list<auto>` exige que TODOS os elementos tenham o
    //   mesmo tipo — e aqui eles são `CampoDeTexto*`, `CaixaDeSelecao*`, etc.
    //   O array com tipo declarado converte cada elemento para `Componente*`.
    Componente* todos[] = {&email_, &cnpj_, &termos_, &empresa_, &enviar_, &status_};
    for (Componente* c : todos) c->definir_mediador(this);
    cnpj_.definir_visivel(false);
    revalidar();
}

bool FormularioDeCadastro::email_valido(const std::string& e) {
    const auto arroba = e.find('@');
    return arroba != std::string::npos && arroba > 0 &&
           e.find('.', arroba) != std::string::npos;
}

void FormularioDeCadastro::notificar(Componente* origem, const std::string& evento) {
    registro_.push_back(fmt::format("{} → {}", origem->nome(), evento));

    // TODA a lógica de interação, num lugar só
    if (origem == &empresa_ && evento == "alternou") {
        // Marcar "empresa" revela o CNPJ e o torna obrigatório
        cnpj_.definir_visivel(empresa_.marcada());
        cnpj_.definir_obrigatorio(empresa_.marcada());
        revalidar();
        return;
    }

    if (evento == "mudou" || evento == "alternou") {
        revalidar();
        return;
    }

    if (origem == &enviar_ && evento == "clicado") {
        status_.definir_texto("enviado com sucesso");
        return;
    }

    if (evento == "clique-ignorado") {
        status_.definir_texto("preencha os campos obrigatórios");
        return;
    }
}

void FormularioDeCadastro::revalidar() {
    const bool email_ok  = email_valido(email_.texto());
    const bool termos_ok = termos_.marcada();
    const bool cnpj_ok   = !cnpj_.obrigatorio() || !cnpj_.texto().empty();

    const bool tudo_ok = email_ok && termos_ok && cnpj_ok;
    enviar_.definir_habilitado(tudo_ok);

    if (!email_ok)        status_.definir_texto("e-mail inválido");
    else if (!termos_ok)  status_.definir_texto("aceite os termos");
    else if (!cnpj_ok)    status_.definir_texto("CNPJ é obrigatório para empresas");
    else                  status_.definir_texto("pronto para enviar");
}

}  // namespace mediator
