#pragma once
// =============================================================================
//  subsistemas.hpp — as peças que a Facade esconde
//
//  Um pipeline de build tem seis subsistemas, cada um com API própria e uma
//  ORDEM obrigatória entre eles. Usá-los direto significa:
//
//    • saber a ordem (resolver deps antes de compilar, compilar antes de linkar)
//    • saber o que passar de um para o outro
//    • lembrar de limpar em caso de falha
//    • conhecer seis cabeçalhos
//
//  Nenhum desses subsistemas é ruim. O problema é o CONJUNTO — e é isso que a
//  Facade resolve.
// =============================================================================
#include <string>
#include <vector>

namespace facade {

// ── 1. Resolução de dependências ─────────────────────────────────────────────
class ResolvedorDeDependencias {
public:
    void adicionar_repositorio(std::string url);
    std::vector<std::string> resolver(const std::vector<std::string>& pedidas);
private:
    std::vector<std::string> repos_;
};

// ── 2. Cache de artefatos ────────────────────────────────────────────────────
class Cache {
public:
    bool tem(const std::string& chave) const;
    void guardar(const std::string& chave, std::size_t bytes);
    std::size_t total_guardado() const { return total_; }
private:
    std::vector<std::string> chaves_;
    std::size_t              total_ = 0;
};

// ── 3. Compilador ────────────────────────────────────────────────────────────
class Compilador {
public:
    void definir_padrao(std::string padrao) { padrao_ = std::move(padrao); }
    void definir_otimizacao(int nivel)      { otimizacao_ = nivel; }
    std::vector<std::string> compilar(const std::vector<std::string>& fontes);
private:
    std::string padrao_     = "c++17";
    int         otimizacao_ = 0;
};

// ── 4. Linker ────────────────────────────────────────────────────────────────
class Linker {
public:
    std::string linkar(const std::vector<std::string>& objetos,
                       const std::vector<std::string>& bibliotecas,
                       const std::string& saida);
};

// ── 5. Executor de testes ────────────────────────────────────────────────────
struct ResultadoDeTestes {
    int passou = 0;
    int falhou = 0;
    bool ok() const { return falhou == 0; }
};

class ExecutorDeTestes {
public:
    ResultadoDeTestes executar(const std::string& binario);
};

// ── 6. Empacotador ───────────────────────────────────────────────────────────
class Empacotador {
public:
    std::string empacotar(const std::string& binario, const std::string& formato);
};

}  // namespace facade
