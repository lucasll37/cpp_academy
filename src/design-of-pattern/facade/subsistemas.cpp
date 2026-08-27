#include "subsistemas.hpp"

#include <fmt/core.h>

#include <algorithm>

namespace facade {

void ResolvedorDeDependencias::adicionar_repositorio(std::string url) {
    fmt::print("      [deps] repositório: {}\n", url);
    repos_.push_back(std::move(url));
}

std::vector<std::string> ResolvedorDeDependencias::resolver(
        const std::vector<std::string>& pedidas) {
    fmt::print("      [deps] resolvendo {} dependência(s) em {} repo(s)\n",
               pedidas.size(), repos_.size());
    std::vector<std::string> r;
    r.reserve(pedidas.size());
    for (const auto& d : pedidas) r.push_back("lib" + d + ".a");
    return r;
}

bool Cache::tem(const std::string& chave) const {
    return std::find(chaves_.begin(), chaves_.end(), chave) != chaves_.end();
}

void Cache::guardar(const std::string& chave, std::size_t bytes) {
    fmt::print("      [cache] guardando '{}' ({} B)\n", chave, bytes);
    chaves_.push_back(chave);
    total_ += bytes;
}

std::vector<std::string> Compilador::compilar(const std::vector<std::string>& fontes) {
    fmt::print("      [cc] compilando {} arquivo(s) com -std={} -O{}\n",
               fontes.size(), padrao_, otimizacao_);
    std::vector<std::string> objs;
    objs.reserve(fontes.size());
    for (const auto& f : fontes) {
        objs.push_back(f.substr(0, f.rfind('.')) + ".o");
    }
    return objs;
}

std::string Linker::linkar(const std::vector<std::string>& objetos,
                           const std::vector<std::string>& bibliotecas,
                           const std::string& saida) {
    fmt::print("      [ld] linkando {} objeto(s) + {} biblioteca(s) → {}\n",
               objetos.size(), bibliotecas.size(), saida);
    return saida;
}

ResultadoDeTestes ExecutorDeTestes::executar(const std::string& binario) {
    fmt::print("      [test] executando suíte de '{}'\n", binario);
    return {12, 0};
}

std::string Empacotador::empacotar(const std::string& binario,
                                   const std::string& formato) {
    const auto nome = binario + "." + formato;
    fmt::print("      [pack] empacotando '{}' → '{}'\n", binario, nome);
    return nome;
}

}  // namespace facade
