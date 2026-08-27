#pragma once
// =============================================================================
//  construtor.hpp — o BUILDER clássico
//
//  A ideia: separar a CONSTRUÇÃO da REPRESENTAÇÃO. Cada passo é um método com
//  nome, e a ordem não importa. O `construir()` valida e entrega o produto.
//
//  Três decisões de design visíveis aqui:
//
//    1. os OBRIGATÓRIOS vão no construtor do builder, não em setters — assim
//       é impossível esquecer, porque o builder não existe sem eles
//
//    2. `construir()` valida UMA vez, quando todos os campos já existem. Um
//       setter público não conseguiria validar regras que dependem de DOIS
//       campos ("csv exige separador"), porque não sabe se o outro já veio
//
//    3. TODOS os métodos são `&&`-qualificados e devolvem `Construtor&&`
//
//  A decisão 3 merece explicação. A tentação é escrever:
//
//      Construtor&  limite(int n)  { ...; return *this; }   // devolve LVALUE
//      Relatorio    construir() && { ... }                  // exige RVALUE
//
//  Isso NÃO COMPILA numa cadeia: o `limite()` devolve um lvalue, e o
//  `construir()` seguinte recusa. Duas saídas:
//
//    (a) sobrecarregar cada setter em `&` e `&&`  → dobra o código
//    (b) tornar TUDO `&&`                          → é o que fazemos aqui
//
//  Com (b), o builder é rvalue do início ao fim — o que casa exatamente com a
//  intenção "construa UMA vez e entregue". Reusar exige um `std::move`
//  explícito, e aí a responsabilidade é de quem escreveu.
//  Ver src/value_semantics/demo_move_only.hpp, seção 5.2.
// =============================================================================
#include "relatorio.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace builder {

class Relatorio::Construtor {
public:
    // Os OBRIGATÓRIOS entram aqui. Não há como construir o builder sem eles.
    Construtor(std::string titulo, std::string formato) {
        r_.titulo_  = std::move(titulo);
        r_.formato_ = std::move(formato);
    }

    // Cada passo opcional é um método com NOME.
    // `&&` + `return std::move(*this)` mantém a cadeia inteira em rvalue.
    Construtor&& colunas(std::vector<std::string> c) && {
        r_.colunas_ = std::move(c);
        return std::move(*this);
    }
    Construtor&& com_cabecalho(bool v = true) && { r_.cabecalho_ = v; return std::move(*this); }
    Construtor&& com_rodape(bool v = true)    && { r_.rodape_ = v;    return std::move(*this); }
    Construtor&& limite(int n)                && { r_.limite_ = n;    return std::move(*this); }
    Construtor&& separador(std::string s)     && { r_.separador_ = std::move(s);   return std::move(*this); }
    Construtor&& codificacao(std::string s)   && { r_.codificacao_ = std::move(s); return std::move(*this); }
    Construtor&& casas_decimais(int n)        && { r_.casas_decimais_ = n; return std::move(*this); }

    // O passo final: valida e entrega.
    Relatorio construir() && {
        validar();
        return std::move(r_);
    }

private:
    void validar() const {
        if (r_.titulo_.empty()) {
            throw std::invalid_argument("Relatorio: título não pode ser vazio");
        }
        if (r_.formato_ != "csv" && r_.formato_ != "json" && r_.formato_ != "html") {
            throw std::invalid_argument("Relatorio: formato deve ser csv, json ou html");
        }
        if (r_.casas_decimais_ < 0 || r_.casas_decimais_ > 10) {
            throw std::invalid_argument("Relatorio: casas_decimais fora de 0..10");
        }
        // A regra que só um `construir()` consegue verificar: ela depende de
        // DOIS campos, e um setter não sabe se o outro já foi definido.
        if (r_.formato_ == "csv" && r_.separador_.empty()) {
            throw std::invalid_argument("Relatorio: csv exige um separador");
        }
    }

    Relatorio r_;
};

}  // namespace builder
