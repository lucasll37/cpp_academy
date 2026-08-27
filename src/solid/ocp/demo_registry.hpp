#pragma once
// =============================================================================
//  demo_registry.hpp — OCP no limite: registro auto-populado
// =============================================================================
//
//  Nos demos anteriores, o consumidor (`area_total`, `Carrinho`) ficou fechado
//  — mas ALGUÉM ainda precisava escrever `formas.push_back(make_unique<...>)`.
//  Esse ponto de composição continua sendo editado a cada extensão.
//
//  O registro resolve isso: cada tipo se INSCREVE sozinho, na inicialização
//  estática. Acrescentar um handler passa a ser literalmente um arquivo novo,
//  sem tocar em NENHUM outro — nem no main.
//
//  É o mecanismo por trás de:
//    • frameworks de teste (TEST_F se registra sozinho)
//    • serializadores por tipo
//    • sistemas de plugin (ver src/plugin_registry/)
//
// =============================================================================

#include <fmt/core.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace demo_registry {

// ─────────────────────────────────────────────────────────────────────────────
//  A interface e o registro
// ─────────────────────────────────────────────────────────────────────────────
class Exportador {
public:
    virtual ~Exportador() = default;
    virtual std::string extensao() const = 0;
    virtual std::string exportar(const std::vector<std::string>& linhas) const = 0;
};

class Registro {
public:
    using Fabrica = std::function<std::unique_ptr<Exportador>()>;

    // Singleton com inicialização thread-safe (Meyers singleton).
    // Precisa ser função, não variável global: a ordem de inicialização
    // entre unidades de tradução não é definida, e os registradores
    // estáticos rodam ANTES do main.
    static Registro& instancia() {
        static Registro r;
        return r;
    }

    void registrar(const std::string& nome, Fabrica f) {
        fabricas_.emplace(nome, std::move(f));
    }

    std::unique_ptr<Exportador> criar(const std::string& nome) const {
        auto it = fabricas_.find(nome);
        return it != fabricas_.end() ? it->second() : nullptr;
    }

    std::vector<std::string> disponiveis() const {
        std::vector<std::string> nomes;
        for (const auto& [nome, _] : fabricas_) nomes.push_back(nome);
        return nomes;
    }

private:
    Registro() = default;
    std::map<std::string, Fabrica> fabricas_;
};

// O truque: um objeto cujo CONSTRUTOR faz a inscrição. Declarar uma instância
// estática dele no escopo de namespace faz a inscrição acontecer antes do main.
template <typename T>
struct AutoRegistro {
    explicit AutoRegistro(const std::string& nome) {
        Registro::instancia().registrar(nome, [] { return std::make_unique<T>(); });
    }
};

// A macro que esconde o boilerplate. `inline` (C++17) evita violação de ODR
// se o header for incluído em várias unidades de tradução.
#define REGISTRAR_EXPORTADOR(Classe, nome) \
    inline const AutoRegistro<Classe> registro_##Classe{nome}

// ─────────────────────────────────────────────────────────────────────────────
//  Os exportadores. Cada bloco abaixo poderia estar num arquivo separado,
//  compilado separadamente, e nada mais no projeto mudaria.
// ─────────────────────────────────────────────────────────────────────────────
class ExportadorCsv final : public Exportador {
public:
    std::string extensao() const override { return ".csv"; }
    std::string exportar(const std::vector<std::string>& linhas) const override {
        std::string r;
        for (const auto& l : linhas) r += l + "\n";
        return r;
    }
};
REGISTRAR_EXPORTADOR(ExportadorCsv, "csv");

class ExportadorJson final : public Exportador {
public:
    std::string extensao() const override { return ".json"; }
    std::string exportar(const std::vector<std::string>& linhas) const override {
        std::string r = "[";
        for (std::size_t i = 0; i < linhas.size(); ++i) {
            r += "\"" + linhas[i] + "\"";
            if (i + 1 < linhas.size()) r += ", ";
        }
        return r + "]";
    }
};
REGISTRAR_EXPORTADOR(ExportadorJson, "json");

// ── A EXTENSÃO: acrescentado depois, sem tocar em nada acima ─────────────────
class ExportadorMarkdown final : public Exportador {
public:
    std::string extensao() const override { return ".md"; }
    std::string exportar(const std::vector<std::string>& linhas) const override {
        std::string r;
        for (const auto& l : linhas) r += "- " + l + "\n";
        return r;
    }
};
REGISTRAR_EXPORTADOR(ExportadorMarkdown, "markdown");

// ─────────────────────────────────────────────────────────────────────────────
//  O consumidor — escrito uma vez, nunca mais editado
// ─────────────────────────────────────────────────────────────────────────────
void demo() {
    fmt::print("  Exportadores registrados (descobertos em runtime):\n");

    const std::vector<std::string> dados{"alfa", "beta", "gama"};

    for (const auto& nome : Registro::instancia().disponiveis()) {
        auto exp = Registro::instancia().criar(nome);
        std::string saida = exp->exportar(dados);
        // corta a saída em uma linha só, para caber no demo
        for (char& c : saida) if (c == '\n') c = ' ';
        fmt::print("     {:<10} ({:<5}) → {}\n", nome, exp->extensao(), saida);
    }

    fmt::print("\n     Este loop NUNCA foi editado. Ele nem conhece os nomes das\n");
    fmt::print("     classes — só pergunta ao registro o que existe.\n");
    fmt::print("     O ExportadorMarkdown foi acrescentado depois, e apareceu\n");
    fmt::print("     aqui sozinho.\n");
}

void avisos() {
    fmt::print("\n  ⚠  O preço do registro automático:\n");
    fmt::print("     • ORDEM de inicialização estática entre TUs não é definida —\n");
    fmt::print("       por isso o registro é um Meyers singleton (função com\n");
    fmt::print("       `static` local), e não uma variável global.\n");
    fmt::print("     • o linker pode DESCARTAR o objeto estático se nada na TU\n");
    fmt::print("       for referenciado (--gc-sections, bibliotecas estáticas).\n");
    fmt::print("       Em lib estática, é preciso --whole-archive ou um símbolo\n");
    fmt::print("       âncora referenciado explicitamente.\n");
    fmt::print("     • o fluxo fica INDIRETO: não dá para achar quem registrou\n");
    fmt::print("       lendo o main. Só use quando a extensibilidade compensar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 3: Registro auto-populado ────────────────────────┐\n");
    demo();
    avisos();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_registry
