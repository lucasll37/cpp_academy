#pragma once
// =============================================================================
//  relatorio.hpp — o PRODUTO que o Builder constrói
//
//  O objeto tem muitos campos, quase todos opcionais, e alguns obrigatórios.
//  É exatamente o cenário em que um construtor com 9 parâmetros vira ilegível:
//
//      Relatorio r{"Vendas", "csv", true, false, true, 100, ",", "utf-8", 2};
//                              // ↑ o que é cada `true`?
// =============================================================================
#include <string>
#include <vector>

namespace builder {

class Relatorio {
public:
    // Só o Builder constrói — o construtor é privado e ele é friend.
    // Isso impede que alguém monte um Relatorio pela metade.
    class Construtor;

    const std::string& titulo() const { return titulo_; }
    const std::string& formato() const { return formato_; }
    const std::vector<std::string>& colunas() const { return colunas_; }
    bool cabecalho() const { return cabecalho_; }
    bool rodape()    const { return rodape_; }
    int  limite()    const { return limite_; }
    const std::string& separador() const { return separador_; }
    const std::string& codificacao() const { return codificacao_; }
    int  casas_decimais() const { return casas_decimais_; }

    std::string descrever() const;

private:
    Relatorio() = default;
    friend class Construtor;

    // Obrigatórios
    std::string titulo_;
    std::string formato_;

    // Opcionais, com defaults sensatos
    std::vector<std::string> colunas_;
    bool        cabecalho_      = true;
    bool        rodape_         = false;
    int         limite_         = 0;          // 0 = sem limite
    std::string separador_      = ",";
    std::string codificacao_    = "utf-8";
    int         casas_decimais_ = 2;
};

}  // namespace builder
