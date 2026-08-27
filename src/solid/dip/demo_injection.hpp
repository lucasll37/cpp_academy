#pragma once
// =============================================================================
//  demo_injection.hpp — DIP: as formas de injetar, e onde o `new` sobra
// =============================================================================
//
//  Inverter a dependência levanta uma pergunta prática: se `ServicoDePedidos`
//  não pode construir um `RepositorioMySql`, QUEM constrói?
//
//  A resposta é sempre a mesma: um ponto no programa que pode conhecer TUDO,
//  porque não contém regra de negócio nenhuma. Esse ponto é a COMPOSITION
//  ROOT — normalmente o `main`, ou um módulo de bootstrap ao lado dele.
//
//  Todo o resto do programa recebe suas dependências prontas. O `new` fica
//  confinado num só lugar.
//
//  Tópicos:
//  • Injeção por construtor, por setter e por parâmetro
//  • Composition root
//  • Injeção estática (template) — DIP com custo zero
//  • ⚠ Service locator: por que ele NÃO é injeção
//
// =============================================================================

#include <fmt/core.h>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo_injection {

// ─────────────────────────────────────────────────────────────────────────────
//  As abstrações, declaradas pela política
// ─────────────────────────────────────────────────────────────────────────────
class Relogio {
public:
    virtual ~Relogio() = default;
    virtual std::string agora() const = 0;
};

class Log {
public:
    virtual ~Log() = default;
    virtual void escrever(const std::string& msg) = 0;
};

// ── Implementações de infraestrutura ────────────────────────────────────────
class RelogioDoSistema final : public Relogio {
public:
    std::string agora() const override { return "2026-08-18T10:30:00"; }
};

class RelogioFixo final : public Relogio {          // para testes
public:
    explicit RelogioFixo(std::string t) : t_(std::move(t)) {}
    std::string agora() const override { return t_; }
private:
    std::string t_;
};

class LogNoConsole final : public Log {
public:
    void escrever(const std::string& msg) override { fmt::print("       {}\n", msg); }
};

class LogEmMemoria final : public Log {             // para testes
public:
    void escrever(const std::string& msg) override { linhas.push_back(msg); }
    std::vector<std::string> linhas;
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Injeção por CONSTRUTOR — a forma padrão
//
//  Vantagem decisiva: o objeto é IMPOSSÍVEL de construir num estado inválido.
//  Se a dependência é obrigatória, ela entra no construtor. Ponto.
// ─────────────────────────────────────────────────────────────────────────────
class Auditoria {
public:
    Auditoria(const Relogio& relogio, Log& log) : relogio_(relogio), log_(log) {}

    void registrar(const std::string& evento) {
        log_.escrever(fmt::format("[{}] {}", relogio_.agora(), evento));
    }

private:
    const Relogio& relogio_;
    Log&           log_;
};

void por_construtor() {
    fmt::print("  1. Injeção por CONSTRUTOR — dependência obrigatória\n");

    RelogioDoSistema relogio;
    LogNoConsole     log;
    Auditoria        auditoria(relogio, log);

    auditoria.registrar("usuário 42 fez login");

    fmt::print("     Não existe Auditoria sem relógio e sem log — o construtor\n");
    fmt::print("     não permite. Estado inválido é INEXPRIMÍVEL.\n");
    fmt::print("     ⚠  guardar REFERÊNCIA exige que a dependência viva mais que\n");
    fmt::print("        o objeto. Se a posse for compartilhada, use shared_ptr;\n");
    fmt::print("        se for exclusiva, unique_ptr. Referência só quando o dono\n");
    fmt::print("        é claramente outro (o composition root, aqui).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Injeção por PARÂMETRO — quando a dependência varia por chamada
// ─────────────────────────────────────────────────────────────────────────────
double calcular_desconto(double total, const std::function<double(double)>& politica) {
    return politica(total);
}

void por_parametro() {
    fmt::print("\n  2. Injeção por PARÂMETRO — varia a cada chamada\n");

    fmt::print("     sem desconto:  {:.2f}\n", calcular_desconto(200, [](double t) { return t; }));
    fmt::print("     black friday:  {:.2f}\n", calcular_desconto(200, [](double t) { return t * 0.7; }));

    fmt::print("     Não guarda estado, não precisa de campo. A forma mais\n");
    fmt::print("     simples — e por isso a primeira a considerar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Composition root — onde o `new` mora
//
//  Um único lugar do programa conhece todas as classes concretas e monta o
//  grafo de objetos. Ele não tem regra de negócio: só fiação.
// ─────────────────────────────────────────────────────────────────────────────
namespace app {

// Guarda os objetos vivos e devolve as referências. É o dono de tudo.
struct Aplicacao {
    std::unique_ptr<Relogio> relogio;
    std::unique_ptr<Log>     log;
    std::unique_ptr<Auditoria> auditoria;

    static Aplicacao montar_producao() {
        Aplicacao a;
        a.relogio   = std::make_unique<RelogioDoSistema>();
        a.log       = std::make_unique<LogNoConsole>();
        a.auditoria = std::make_unique<Auditoria>(*a.relogio, *a.log);
        return a;
    }

    static Aplicacao montar_teste() {
        Aplicacao a;
        a.relogio   = std::make_unique<RelogioFixo>("2000-01-01T00:00:00");
        a.log       = std::make_unique<LogEmMemoria>();
        a.auditoria = std::make_unique<Auditoria>(*a.relogio, *a.log);
        return a;
    }
};

} // namespace app

void composition_root() {
    fmt::print("\n  3. Composition root — o único lugar com `new`\n");

    fmt::print("     montar_producao():\n");
    auto prod = app::Aplicacao::montar_producao();
    prod.auditoria->registrar("pedido 7 confirmado");

    fmt::print("     montar_teste():\n");
    auto teste = app::Aplicacao::montar_teste();
    teste.auditoria->registrar("evento de teste");

    auto* log_teste = dynamic_cast<LogEmMemoria*>(teste.log.get());
    fmt::print("       capturado em memória: '{}'\n", log_teste->linhas.front());
    fmt::print("       tempo determinístico → asserção exata é possível\n");

    fmt::print("\n     `Auditoria` é o MESMO objeto nos dois. Só a fiação mudou.\n");
    fmt::print("     Todo `new` do programa está nestas duas funções.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Injeção ESTÁTICA — DIP com custo zero
//
//  Quando a escolha é conhecida em compilação, o template dá a mesma inversão
//  sem vtable e sem alocação. A abstração vira um CONCEPT.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
concept RelogioEstatico = requires(const T t) {
    { t.agora() } -> std::convertible_to<std::string>;
};

struct RelogioRapido {
    std::string agora() const { return "2026-08-18T10:30:00"; }
};

struct RelogioDeTeste {
    std::string agora() const { return "1970-01-01T00:00:00"; }
};

template <RelogioEstatico R>
class AuditoriaEstatica {
public:
    explicit AuditoriaEstatica(R relogio) : relogio_(std::move(relogio)) {}
    std::string registrar(const std::string& evento) const {
        return fmt::format("[{}] {}", relogio_.agora(), evento);
    }
private:
    R relogio_;                      // por VALOR: sem indireção, sem heap
};

void injecao_estatica() {
    fmt::print("\n  4. Injeção ESTÁTICA — a mesma inversão, custo zero\n");

    AuditoriaEstatica prod(RelogioRapido{});
    AuditoriaEstatica teste(RelogioDeTeste{});

    fmt::print("     produção: {}\n", prod.registrar("login"));
    fmt::print("     teste:    {}\n", teste.registrar("login"));

    fmt::print("     sizeof(AuditoriaEstatica<RelogioRapido>) = {} byte(s)\n",
               sizeof(AuditoriaEstatica<RelogioRapido>));
    fmt::print("     Sem vtable, sem ponteiro, sem alocação. A dependência\n");
    fmt::print("     continua invertida — o concept é a abstração.\n");
    fmt::print("     ⚠  em troca: os tipos são diferentes, então não dá para\n");
    fmt::print("        trocar a implementação em runtime nem guardar num vector.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. ⚠ Service locator — o anti-padrão que se disfarça de DIP
// ─────────────────────────────────────────────────────────────────────────────
void serviceLocator() {
    fmt::print("\n  5. ⚠ Service locator NÃO é injeção de dependência\n");

    fmt::print("     O padrão:\n");
    fmt::print("       class Servico {{\n");
    fmt::print("         void fazer() {{\n");
    fmt::print("           auto* log = Locator::obter<Log>();   // ← busca sozinho\n");
    fmt::print("           log->escrever(\"…\");\n");
    fmt::print("         }}\n");
    fmt::print("       }};\n");

    fmt::print("\n     Parece resolver, e é pior que o acoplamento direto:\n");
    fmt::print("       • as dependências ficam INVISÍVEIS na assinatura —\n");
    fmt::print("         você só descobre o que a classe precisa lendo o corpo\n");
    fmt::print("       • o teste vira global mutável: configurar e limpar o\n");
    fmt::print("         locator entre casos, com ordem importando\n");
    fmt::print("       • falha em RUNTIME se algo não foi registrado; a injeção\n");
    fmt::print("         por construtor falha em COMPILAÇÃO\n");
    fmt::print("       • acoplou tudo ao próprio Locator, que agora é global\n");

    fmt::print("\n     A regra que separa os dois:\n");
    fmt::print("       injeção      → a dependência é EMPURRADA para o objeto\n");
    fmt::print("       service locator → o objeto vai BUSCAR a dependência\n");
    fmt::print("     Se a classe busca, ela conhece o buscador. Não houve inversão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 2: Formas de injeção e composition root ──────────┐\n");
    por_construtor();
    por_parametro();
    composition_root();
    injecao_estatica();
    serviceLocator();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_injection
