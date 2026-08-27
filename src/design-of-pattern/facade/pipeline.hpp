#pragma once
// =============================================================================
//  pipeline.hpp — A FACADE
//
//  Uma interface simples para um subsistema complexo. Três pontos de design:
//
//    1. a Facade NÃO ESCONDE os subsistemas — quem precisar do controle fino
//       ainda pode usá-los direto. Ela é uma CONVENIÊNCIA, não uma prisão.
//       (esconder de verdade seria o padrão Mediator ou uma camada de ACL)
//
//    2. ela codifica a ORDEM e o fluxo de dados entre os subsistemas — que é
//       justamente o conhecimento que estava espalhado por todo cliente
//
//    3. ela não acrescenta funcionalidade nova. Se você se pegar escrevendo
//       lógica de negócio na Facade, provavelmente ela virou outra coisa.
// =============================================================================
#include "subsistemas.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace facade {

enum class ErroDeBuild { CompilacaoFalhou, TestesFalharam, SemFontes };

std::string_view descrever(ErroDeBuild e);

struct Opcoes {
    std::string              padrao      = "c++20";
    int                      otimizacao  = 2;
    bool                     rodar_testes = true;
    std::string              formato_pacote = "tar.gz";
    std::vector<std::string> dependencias;
};

class PipelineDeBuild {
public:
    PipelineDeBuild();

    // A UMA chamada que substitui as seis
    std::expected<std::string, ErroDeBuild>
    construir(const std::string& projeto,
              const std::vector<std::string>& fontes,
              const Opcoes& opcoes = {});

    std::size_t bytes_em_cache() const { return cache_.total_guardado(); }

private:
    ResolvedorDeDependencias deps_;
    Cache                    cache_;
    Compilador               cc_;
    Linker                   ld_;
    ExecutorDeTestes         testes_;
    Empacotador              pack_;
};

}  // namespace facade
