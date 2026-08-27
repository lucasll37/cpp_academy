// =============================================================================
//  src/testing/carrinho.hpp
//
//  O sistema sob teste
//  ───────────────────
//  Um carrinho de compras pequeno o bastante para caber na cabeça e grande o
//  bastante para ter os problemas que testes de verdade encontram:
//
//    • uma REGRA de negócio com faixas (o desconto por quantidade);
//    • uma PRÉ-CONDIÇÃO que aborta (para o death test);
//    • uma EXCEÇÃO em caso de erro (para EXPECT_THROW);
//    • uma DEPENDÊNCIA externa (o repositório de preços) — a razão de existir
//      dos dublês de teste;
//    • aritmética de PONTO FLUTUANTE, que não se compara com ==.
//
//  Repare que a dependência entra por uma INTERFACE recebida no construtor.
//  Isso não é enfeite: é o que torna a classe testável sem banco de dados.
//  É o Princípio da Inversão de Dependência — ver src/solid/dip/.
// =============================================================================
#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace loja {

// ── A dependência, como INTERFACE ───────────────────────────────────────────
class RepositorioDePrecos {
public:
    virtual ~RepositorioDePrecos() = default;
    // devolve nullopt quando o produto não existe
    virtual std::optional<double> preco_de(const std::string& sku) const = 0;
};

// ── Exceção do domínio ──────────────────────────────────────────────────────
class ProdutoDesconhecido : public std::runtime_error {
public:
    explicit ProdutoDesconhecido(const std::string& sku)
        : std::runtime_error{"produto desconhecido: " + sku} {}
};

struct Item {
    std::string sku;
    int         quantidade;
    double      preco_unitario;
};

class Carrinho {
public:
    explicit Carrinho(const RepositorioDePrecos& precos) : precos_{precos} {}

    // Lança ProdutoDesconhecido se o sku não existir no repositório.
    // ABORTA (via a pré-condição) se quantidade <= 0 — é um erro de PROGRAMA,
    // não de entrada, e a distinção é o assunto de src/errors/.
    void adicionar(const std::string& sku, int quantidade);

    void   remover(const std::string& sku);
    bool   vazio() const noexcept { return itens_.empty(); }
    std::size_t tamanho() const noexcept { return itens_.size(); }

    double subtotal() const noexcept;

    // Desconto por quantidade TOTAL de peças:
    //     < 10 peças ......  0%
    //    10..49 peças ......  5%
    //    50..99 peças ...... 10%
    //   >= 100 peças ...... 15%
    double percentual_de_desconto() const noexcept;
    double total() const noexcept;

    const std::vector<Item>& itens() const noexcept { return itens_; }

private:
    const RepositorioDePrecos& precos_;
    std::vector<Item>          itens_;
};

}  // namespace loja
