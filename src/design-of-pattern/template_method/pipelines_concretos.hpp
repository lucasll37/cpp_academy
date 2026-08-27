#pragma once
// =============================================================================
//  pipelines_concretos.hpp — três variações do MESMO esqueleto
//
//  Repare em quanto cada uma escreve: só os passos que ela precisa mudar.
//  A ordem — extrair, validar, transformar, ordenar — vem de graça.
// =============================================================================
#include "pipeline.hpp"

#include <string>
#include <vector>

namespace template_method {

// Só o obrigatório: dois métodos
class PipelineCsv final : public PipelineDeDados {
public:
    PipelineCsv() : PipelineDeDados("CSV") {}

protected:
    std::vector<Registro> extrair(const std::string& fonte) override;
    void                  transformar(Registro& r) override;
};

// Sobrescreve um HOOK além dos obrigatórios
class PipelineJson final : public PipelineDeDados {
public:
    PipelineJson() : PipelineDeDados("JSON") {}

protected:
    std::vector<Registro> extrair(const std::string& fonte) override;
    void                  transformar(Registro& r) override;

    // Hook: este pipeline rejeita valores negativos
    bool validar(const Registro& r) const override;
};

// Usa todos os hooks
class PipelineBanco final : public PipelineDeDados {
public:
    PipelineBanco() : PipelineDeDados("Banco") {}

    bool conectado() const { return conectado_; }

protected:
    std::vector<Registro> extrair(const std::string& fonte) override;
    void                  transformar(Registro& r) override;

    bool validar(const Registro& r) const override;
    void antes_de_tudo() override;
    void depois_de_tudo() override;
    bool deve_ordenar() const override { return true; }

private:
    bool conectado_ = false;
};

}  // namespace template_method
