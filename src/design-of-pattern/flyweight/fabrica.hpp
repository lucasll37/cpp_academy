#pragma once
// =============================================================================
//  fabrica.hpp — a FÁBRICA que garante o compartilhamento
//
//  Sem ela o padrão não funciona: é ela que garante que pedir o mesmo glifo
//  duas vezes devolve o MESMO objeto, não uma cópia.
//
//  Três decisões:
//    1. a chave é a tupla do estado intrínseco INTEIRO ('a' + fonte + tamanho)
//    2. devolve `const Glifo&` — o cliente NÃO pode modificar o compartilhado
//    3. `deque` em vez de `vector` para os glifos: `vector` realoca e
//       invalidaria as referências já entregues (src/stl/, 1.3)
// =============================================================================
#include "glifo.hpp"

#include <deque>
#include <map>
#include <string>
#include <tuple>

namespace flyweight {

class FabricaDeGlifos {
public:
    // Devolve SEMPRE a mesma instância para a mesma chave
    const Glifo& obter(char c, const std::string& fonte, int tamanho);

    std::size_t distintos() const { return indice_.size(); }
    std::size_t bytes_totais() const;

private:
    using Chave = std::tuple<char, std::string, int>;

    // `deque` NÃO invalida referências ao crescer no fim — `vector` invalidaria,
    // e as referências já entregues virariam ponteiros pendurados.
    std::deque<Glifo>        glifos_;
    std::map<Chave, std::size_t> indice_;
};

}  // namespace flyweight
