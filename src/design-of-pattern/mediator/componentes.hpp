#pragma once
// =============================================================================
//  componentes.hpp — os widgets de um formulário
//
//  A regra de negócio: o botão "Enviar" só habilita quando o e-mail é válido
//  E os termos foram aceitos. Marcar "empresa" revela um campo extra e torna
//  o CNPJ obrigatório.
//
//  Sem mediador, essa regra ficaria espalhada: o campo de e-mail teria que
//  conhecer o botão e a caixa de termos; a caixa teria que conhecer o botão e
//  o campo de CNPJ; e assim por diante.
//
//  Repare que NENHUM componente abaixo menciona outro.
// =============================================================================
#include "mediador.hpp"

#include <string>

namespace mediator {

class CampoDeTexto final : public Componente {
public:
    using Componente::Componente;

    void digitar(const std::string& texto) {
        texto_ = texto;
        emitir("mudou");                       // só avisa o mediador
    }

    const std::string& texto() const { return texto_; }

    void definir_visivel(bool v) { visivel_ = v; }
    bool visivel() const { return visivel_; }

    void definir_obrigatorio(bool v) { obrigatorio_ = v; }
    bool obrigatorio() const { return obrigatorio_; }

private:
    std::string texto_;
    bool        visivel_     = true;
    bool        obrigatorio_ = false;
};

class CaixaDeSelecao final : public Componente {
public:
    using Componente::Componente;

    void alternar() {
        marcada_ = !marcada_;
        emitir("alternou");
    }

    bool marcada() const { return marcada_; }

private:
    bool marcada_ = false;
};

class Botao final : public Componente {
public:
    using Componente::Componente;

    void clicar() {
        if (!habilitado_) { emitir("clique-ignorado"); return; }
        emitir("clicado");
    }

    void definir_habilitado(bool v) { habilitado_ = v; }
    bool habilitado() const { return habilitado_; }

private:
    bool habilitado_ = false;
};

class Rotulo final : public Componente {
public:
    using Componente::Componente;
    void definir_texto(std::string t) { texto_ = std::move(t); }
    const std::string& texto() const { return texto_; }
private:
    std::string texto_;
};

}  // namespace mediator
