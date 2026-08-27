// =============================================================================
//  src/testing/carrinho.cpp — a implementação do sistema sob teste
// =============================================================================
#include "carrinho.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>

namespace loja {

void Carrinho::adicionar(const std::string& sku, int quantidade) {
    // ⚠ Pré-condição: chamar com quantidade <= 0 é um BUG DO CHAMADOR, não uma
    //   entrada inválida do usuário. `assert` documenta e detecta em debug;
    //   é o que o death test de test_death.cpp exercita.
    //   Ver src/errors/ sobre a diferença entre erro e bug.
    assert(quantidade > 0 && "quantidade precisa ser positiva");

    const auto preco = precos_.preco_de(sku);
    if (!preco) throw ProdutoDesconhecido{sku};      // ← erro de ENTRADA: exceção

    // Se o sku já está no carrinho, soma à quantidade existente.
    auto it = std::find_if(itens_.begin(), itens_.end(),
                           [&sku](const Item& i) { return i.sku == sku; });
    if (it != itens_.end()) it->quantidade += quantidade;
    else                    itens_.push_back(Item{sku, quantidade, *preco});
}

void Carrinho::remover(const std::string& sku) {
    std::erase_if(itens_, [&sku](const Item& i) { return i.sku == sku; });
}

double Carrinho::subtotal() const noexcept {
    return std::accumulate(itens_.begin(), itens_.end(), 0.0,
                           [](double acc, const Item& i) {
                               return acc + i.quantidade * i.preco_unitario;
                           });
}

double Carrinho::percentual_de_desconto() const noexcept {
    const int pecas = std::accumulate(itens_.begin(), itens_.end(), 0,
                                      [](int acc, const Item& i) {
                                          return acc + i.quantidade;
                                      });
    if (pecas >= 100) return 0.15;
    if (pecas >=  50) return 0.10;
    if (pecas >=  10) return 0.05;
    return 0.0;
}

double Carrinho::total() const noexcept {
    return subtotal() * (1.0 - percentual_de_desconto());
}

}  // namespace loja
