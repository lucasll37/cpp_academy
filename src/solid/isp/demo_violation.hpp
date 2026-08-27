#pragma once
// =============================================================================
//  demo_violation.hpp — ISP: violação vs. conformidade
// =============================================================================
//
//  Interface Segregation Principle, de Robert C. Martin:
//
//    "Clients should not be forced to depend upon interfaces that they
//     do not use."
//
//  A "fat interface" é o anti-padrão: uma interface que junta tudo que
//  qualquer implementador PODERIA fazer. O resultado é previsível —
//  metade dos implementadores acaba com métodos que não fazem sentido,
//  e as saídas são todas ruins:
//
//    • lançar exceção          → viola o LSP
//    • não fazer nada          → pior: quebra em silêncio
//    • devolver valor inventado→ pior ainda: corrompe dados
//
//  Repare que ISP e LSP são o mesmo problema visto de dois lados:
//    LSP: "o derivado consegue cumprir tudo que a base promete?"
//    ISP: "a base está prometendo coisas demais?"
//
// =============================================================================

#include <fmt/core.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace demo_violation {

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO — a "fat interface" de uma multifuncional
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

class Multifuncional {
public:
    virtual ~Multifuncional() = default;
    virtual void imprimir(const std::string& doc) = 0;
    virtual void digitalizar(const std::string& destino) = 0;
    virtual void enviar_fax(const std::string& numero) = 0;
    virtual void grampear(int folhas) = 0;
};

// A multifuncional de verdade implementa tudo — sem problema.
class MfpProfissional final : public Multifuncional {
public:
    void imprimir(const std::string& doc) override    { fmt::print("       imprimindo '{}'\n", doc); }
    void digitalizar(const std::string& d) override    { fmt::print("       digitalizando → {}\n", d); }
    void enviar_fax(const std::string& n) override     { fmt::print("       fax para {}\n", n); }
    void grampear(int f) override                      { fmt::print("       grampeando {} folhas\n", f); }
};

// Uma impressora simples é FORÇADA a declarar três métodos que não sabe fazer.
class ImpressoraSimples final : public Multifuncional {
public:
    void imprimir(const std::string& doc) override { fmt::print("       imprimindo '{}'\n", doc); }

    // Saída 1: lançar. Honesto, mas viola o LSP — o cliente quebra.
    void digitalizar(const std::string&) override {
        throw std::logic_error("esta impressora não digitaliza");
    }
    // Saída 2: não fazer nada. Pior: o cliente acha que funcionou.
    void enviar_fax(const std::string&) override { /* silêncio */ }

    // Saída 3: fingir. Pior ainda.
    void grampear(int) override { fmt::print("       (fingindo grampear)\n"); }
};

void demo() {
    fmt::print("  ❌ VIOLAÇÃO — uma interface \"gorda\"\n");

    std::vector<std::unique_ptr<Multifuncional>> frota;
    frota.push_back(std::make_unique<MfpProfissional>());
    frota.push_back(std::make_unique<ImpressoraSimples>());

    const char* nomes[] = {"MfpProfissional", "ImpressoraSimples"};

    for (std::size_t i = 0; i < frota.size(); ++i) {
        fmt::print("     {}:\n", nomes[i]);
        frota[i]->imprimir("relatorio.pdf");
        try {
            frota[i]->digitalizar("/tmp/scan.png");
        } catch (const std::exception& e) {
            fmt::print("       ✗ EXCEÇÃO: {}\n", e.what());
        }
        frota[i]->enviar_fax("11-99999-0000");
        fmt::print("       ↑ o fax \"funcionou\"? não dá para saber.\n");
    }

    fmt::print("\n     Três problemas, todos causados pela interface gorda:\n");
    fmt::print("       • ImpressoraSimples depende de 3 métodos que não usa\n");
    fmt::print("       • o cliente não tem como saber o que é suportado\n");
    fmt::print("       • acrescentar `virtual void encadernar()` na interface\n");
    fmt::print("         RECOMPILA e quebra TODOS os implementadores\n");
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — interfaces pequenas, compostas por herança múltipla
//
//  Cada capacidade vira uma interface própria. O implementador declara
//  exatamente o que sabe fazer, e o CLIENTE pede exatamente o que precisa.
//  A herança múltipla de interfaces puras é o caso seguro (ver src/oop/).
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

class Imprimivel {
public:
    virtual ~Imprimivel() = default;
    virtual void imprimir(const std::string& doc) = 0;
};

class Digitalizavel {
public:
    virtual ~Digitalizavel() = default;
    virtual void digitalizar(const std::string& destino) = 0;
};

class Faxeavel {
public:
    virtual ~Faxeavel() = default;
    virtual void enviar_fax(const std::string& numero) = 0;
};

// Declara SÓ o que sabe fazer
class ImpressoraSimples final : public Imprimivel {
public:
    void imprimir(const std::string& doc) override {
        fmt::print("       [simples] imprimindo '{}'\n", doc);
    }
};

// Compõe as capacidades que tem
class MfpProfissional final : public Imprimivel, public Digitalizavel, public Faxeavel {
public:
    void imprimir(const std::string& doc) override {
        fmt::print("       [mfp] imprimindo '{}'\n", doc);
    }
    void digitalizar(const std::string& d) override {
        fmt::print("       [mfp] digitalizando → {}\n", d);
    }
    void enviar_fax(const std::string& n) override {
        fmt::print("       [mfp] fax para {}\n", n);
    }
};

// Um scanner de mesa: nem imprime, nem faxeia. E isso é dizível agora.
class ScannerDeMesa final : public Digitalizavel {
public:
    void digitalizar(const std::string& d) override {
        fmt::print("       [scanner] digitalizando → {}\n", d);
    }
};

// ── O ponto principal: o CLIENTE pede o mínimo de que precisa ────────────────
// Esta função aceita QUALQUER coisa que imprima — inclusive tipos que ainda
// não existem. E não obriga ninguém a saber digitalizar.
void fluxo_de_impressao(Imprimivel& dispositivo, const std::string& doc) {
    dispositivo.imprimir(doc);
}

void fluxo_de_arquivamento(Digitalizavel& dispositivo, const std::string& destino) {
    dispositivo.digitalizar(destino);
}

void demo() {
    fmt::print("  ✅ CONFORMIDADE — interfaces pequenas e compostas\n");

    ImpressoraSimples simples;
    MfpProfissional   mfp;
    ScannerDeMesa     scanner;

    fmt::print("     fluxo_de_impressao(Imprimivel&):\n");
    fluxo_de_impressao(simples, "nota.pdf");
    fluxo_de_impressao(mfp,     "contrato.pdf");
    // fluxo_de_impressao(scanner, "x");     // ✗ NÃO COMPILA — e é isso que queremos

    fmt::print("     fluxo_de_arquivamento(Digitalizavel&):\n");
    fluxo_de_arquivamento(mfp,     "/tmp/a.png");
    fluxo_de_arquivamento(scanner, "/tmp/b.png");
    // fluxo_de_arquivamento(simples, "x");  // ✗ NÃO COMPILA

    fmt::print("\n     O ganho decisivo: `fluxo_de_impressao(scanner, ...)` é\n");
    fmt::print("     ERRO DE COMPILAÇÃO. Na versão gorda, era exceção em runtime\n");
    fmt::print("     — ou, pior, silêncio.\n");
    fmt::print("     E acrescentar uma interface `Encadernavel` não recompila\n");
    fmt::print("     NEM toca em nada que já existe.\n");
}

} // namespace good

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 1: Violação vs. Conformidade ─────────────────────┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_violation
