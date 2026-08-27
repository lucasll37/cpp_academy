#include "pipeline.hpp"

#include <fmt/core.h>

namespace facade {

std::string_view descrever(ErroDeBuild e) {
    switch (e) {
        case ErroDeBuild::CompilacaoFalhou: return "a compilação falhou";
        case ErroDeBuild::TestesFalharam:   return "os testes falharam";
        case ErroDeBuild::SemFontes:        return "nenhum arquivo-fonte";
    }
    return "?";
}

PipelineDeBuild::PipelineDeBuild() {
    // A Facade também esconde a CONFIGURAÇÃO inicial dos subsistemas
    deps_.adicionar_repositorio("https://conan.io/center");
}

std::expected<std::string, ErroDeBuild> PipelineDeBuild::construir(
        const std::string& projeto,
        const std::vector<std::string>& fontes,
        const Opcoes& opcoes) {

    if (fontes.empty()) return std::unexpected{ErroDeBuild::SemFontes};

    // 1. Resolver dependências — TEM que vir antes de compilar
    const auto bibliotecas = deps_.resolver(opcoes.dependencias);

    // 2. Configurar o compilador com as opções
    cc_.definir_padrao(opcoes.padrao);
    cc_.definir_otimizacao(opcoes.otimizacao);

    // 3. Compilar
    const auto objetos = cc_.compilar(fontes);
    if (objetos.empty()) return std::unexpected{ErroDeBuild::CompilacaoFalhou};

    // 4. Guardar no cache
    for (const auto& o : objetos) cache_.guardar(o, 1024);

    // 5. Linkar
    const auto binario = ld_.linkar(objetos, bibliotecas, projeto);

    // 6. Testar (opcional)
    if (opcoes.rodar_testes) {
        const auto r = testes_.executar(binario);
        fmt::print("      [test] {} passaram, {} falharam\n", r.passou, r.falhou);
        if (!r.ok()) return std::unexpected{ErroDeBuild::TestesFalharam};
    }

    // 7. Empacotar
    return pack_.empacotar(binario, opcoes.formato_pacote);
}

}  // namespace facade
