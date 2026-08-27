#pragma once
// =============================================================================
//  pipeline.hpp — o ESQUELETO e os pontos de variação
//
//  O Template Method define o ESQUELETO de um algoritmo na base, e deixa os
//  passos variáveis para os derivados. A ORDEM e a estrutura ficam na base;
//  só o conteúdo dos passos varia.
//
//  A "inversão de controle" que caracteriza o padrão:
//    Strategy         → o cliente CHAMA a estratégia
//    Template Method  → a BASE chama o derivado ("não nos chame, nós chamamos")
//
//  Três tipos de passo, e a distinção importa:
//    virtual puro   → OBRIGATÓRIO: o derivado tem que implementar
//    virtual normal → HOOK: tem default, o derivado pode sobrescrever
//    não-virtual    → FIXO: o derivado não pode mudar
// =============================================================================
#include <string>
#include <vector>

namespace template_method {

struct Registro {
    std::string chave;
    double      valor;
    bool        valido = true;
};

class PipelineDeDados {
public:
    virtual ~PipelineDeDados() = default;

    // ── O TEMPLATE METHOD ────────────────────────────────────────────────────
    //  Ele é NÃO-VIRTUAL de propósito: a ORDEM dos passos é a coisa que o
    //  padrão protege. Um derivado que pudesse sobrescrevê-lo destruiria a
    //  garantia. É o idioma NVI aplicado — ver src/solid/lsp/, 3.2.
    std::vector<Registro> executar(const std::string& fonte);

    const std::string& nome() const { return nome_; }

protected:
    explicit PipelineDeDados(std::string nome) : nome_(std::move(nome)) {}

    // ── Passos OBRIGATÓRIOS ──────────────────────────────────────────────────
    virtual std::vector<Registro> extrair(const std::string& fonte) = 0;
    virtual void                  transformar(Registro& r) = 0;

    // ── HOOKS: têm default, o derivado escolhe se sobrescreve ────────────────
    //  É a diferença entre "você PRECISA" e "você PODE".
    virtual bool validar(const Registro& r) const { return r.valido; }
    virtual void antes_de_tudo()  {}
    virtual void depois_de_tudo() {}
    virtual bool deve_ordenar() const { return false; }

    // Estatísticas que a base mantém, e o derivado consulta
    int lidos_    = 0;
    int rejeitados_ = 0;

private:
    // Passo FIXO: nenhum derivado muda como se ordena
    void ordenar(std::vector<Registro>& v) const;

    std::string nome_;
};

}  // namespace template_method
