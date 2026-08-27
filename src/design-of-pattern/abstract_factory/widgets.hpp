#pragma once
// =============================================================================
//  widgets.hpp — as FAMÍLIAS de produtos
//
//  O Abstract Factory existe para um problema que a Factory simples não
//  resolve: garantir que os objetos criados sejam COMPATÍVEIS ENTRE SI.
//
//  Uma Factory simples cria UM produto:
//      criar_botao("dark") → BotaoDark
//      criar_campo("light") → CampoLight     ← ninguém impede essa mistura
//
//  Um Abstract Factory cria uma FAMÍLIA inteira, e a família é escolhida uma
//  vez só. Misturar temas passa a ser impossível por construção.
// =============================================================================
#include <memory>
#include <string>

namespace abstract_factory {

// ── Os produtos abstratos ────────────────────────────────────────────────────
class Botao {
public:
    virtual ~Botao() = default;
    virtual std::string desenhar() const = 0;
    virtual std::string ao_clicar() const = 0;
};

class CampoDeTexto {
public:
    virtual ~CampoDeTexto() = default;
    virtual std::string desenhar(const std::string& conteudo) const = 0;
};

class CaixaDeSelecao {
public:
    virtual ~CaixaDeSelecao() = default;
    virtual std::string desenhar(bool marcada) const = 0;
};

// ── A fábrica abstrata: cria a FAMÍLIA inteira ───────────────────────────────
class FabricaDeWidgets {
public:
    virtual ~FabricaDeWidgets() = default;

    virtual std::unique_ptr<Botao>          criar_botao(std::string rotulo) const = 0;
    virtual std::unique_ptr<CampoDeTexto>   criar_campo() const = 0;
    virtual std::unique_ptr<CaixaDeSelecao> criar_caixa() const = 0;

    virtual std::string nome_do_tema() const = 0;
};

}  // namespace abstract_factory
