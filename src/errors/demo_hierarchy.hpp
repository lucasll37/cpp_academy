// =============================================================================
//  src/errors/demo_hierarchy.hpp
//
//  A hierarquia de exceções padrão e como estender
//  ───────────────────────────────────────────────
//  A biblioteca padrão já define uma árvore de exceções. Derivar dela não é
//  formalidade: é o que permite que uma camada genérica escreva
//  `catch (const std::exception&)` e trate qualquer erro do sistema — inclusive
//  os que você inventou depois.
//
//    std::exception
//    ├── std::logic_error         ← erro de PROGRAMAÇÃO (o chamador errou)
//    │   ├── std::invalid_argument
//    │   ├── std::domain_error
//    │   ├── std::length_error
//    │   └── std::out_of_range
//    ├── std::runtime_error       ← erro de EXECUÇÃO (o mundo errou)
//    │   ├── std::range_error
//    │   ├── std::overflow_error
//    │   ├── std::underflow_error
//    │   └── std::system_error
//    ├── std::bad_alloc
//    ├── std::bad_cast
//    └── std::bad_optional_access, std::bad_variant_access, ...
//
//  Tópicos:
//  • logic_error vs runtime_error — a distinção que importa
//  • Exceção customizada carregando contexto
//  • Uma raiz própria por domínio
//  • std::nested_exception — encadeando causas
//  • O que NÃO fazer
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <exception>
#include <functional>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace demo_hierarchy {

// ─────────────────────────────────────────────────────────────────────────────
//  1. logic_error vs runtime_error
//
//     A pergunta que separa os dois: "isto poderia ter sido evitado por quem
//     chamou, verificando antes?"
//       • SIM → logic_error   — é bug. Idealmente vira assert/contrato.
//       • NÃO → runtime_error — o mundo externo falhou. Trate e siga.
// ─────────────────────────────────────────────────────────────────────────────
int elemento_em(const std::vector<int>& v, std::size_t i) {
    if (i >= v.size()) {
        // O chamador PODIA ter checado v.size() antes → erro de lógica
        throw std::out_of_range(
            fmt::format("índice {} fora de [0,{})", i, v.size()));
    }
    return v[i];
}

void conectar(const std::string& host) {
    if (host != "localhost") {
        // Ninguém pode garantir de antemão que a rede vai responder → runtime
        throw std::runtime_error(fmt::format("host '{}' inacessível", host));
    }
}

void demo_logic_vs_runtime() {
    fmt::print("\n── 3.1 logic_error vs runtime_error ──\n");

    std::vector<int> v{10, 20, 30};

    try { elemento_em(v, 7); }
    catch (const std::logic_error& e) {
        fmt::print("      logic_error   : {}\n", e.what());
        fmt::print("                      → é BUG. Corrija o chamador.\n");
    }

    try { conectar("api.exemplo.com"); }
    catch (const std::runtime_error& e) {
        fmt::print("      runtime_error : {}\n", e.what());
        fmt::print("                      → é o mundo. Tente de novo, faça fallback.\n");
    }

    fmt::print("      A distinção guia o TRATAMENTO: logic_error normalmente\n");
    fmt::print("      não deve ser capturado em produção — deixe estourar e\n");
    fmt::print("      corrija o código. runtime_error você trata.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Exceção customizada com contexto
//
//     `what()` devolve `const char*`, então a mensagem precisa sobreviver ao
//     objeto. Derivar de std::runtime_error resolve isso de graça — ele já
//     guarda a string internamente (com contagem de referência, inclusive).
//
//     ⚠ Não guarde std::string como membro e devolva `.c_str()` de um `what()`
//       que constrói a mensagem na hora: o buffer morreria junto com o temporário.
// ─────────────────────────────────────────────────────────────────────────────
class ErroDeConfiguracao : public std::runtime_error {
public:
    ErroDeConfiguracao(std::string arquivo, int linha, const std::string& detalhe)
        : std::runtime_error(                       // a base guarda a mensagem
              fmt::format("{}:{}: {}", arquivo, linha, detalhe)),
          arquivo_(std::move(arquivo)),
          linha_(linha) {}

    // Dados estruturados, para quem quiser tratar programaticamente
    const std::string& arquivo() const noexcept { return arquivo_; }
    int                linha()   const noexcept { return linha_; }

private:
    std::string arquivo_;
    int         linha_;
};

void carregar_config(const std::string& caminho) {
    if (caminho.find(".yaml") == std::string::npos) {
        throw ErroDeConfiguracao(caminho, 1, "extensão não suportada");
    }
}

void demo_customizada() {
    fmt::print("\n── 3.2 Exceção customizada com contexto ──\n");

    try {
        carregar_config("config.ini");
    }
    catch (const ErroDeConfiguracao& e) {
        // Tratamento específico: temos os dados estruturados
        fmt::print("      what()    : {}\n", e.what());
        fmt::print("      arquivo() : {}\n", e.arquivo());
        fmt::print("      linha()   : {}\n", e.linha());
    }

    // E a mesma exceção continua tratável genericamente:
    try {
        carregar_config("config.ini");
    }
    catch (const std::exception& e) {
        fmt::print("      via std::exception&: {}\n", e.what());
    }

    fmt::print("      Derive de std::runtime_error: ele já gerencia a string\n");
    fmt::print("      de `what()`, que precisa sobreviver ao objeto lançado.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Uma raiz própria por domínio
//
//     Um projeto grande ganha uma raiz intermediária: permite
//     `catch (const ErroDaBiblioteca&)` para pegar tudo que é SEU sem pegar
//     um std::bad_alloc do sistema.
// ─────────────────────────────────────────────────────────────────────────────
struct ErroDaAcademia : std::runtime_error {
    using std::runtime_error::runtime_error;     // herda os construtores
};

struct ErroDeParse : ErroDaAcademia {
    using ErroDaAcademia::ErroDaAcademia;
};

struct ErroDeRede : ErroDaAcademia {
    int codigo;
    ErroDeRede(int c, const std::string& msg)
        : ErroDaAcademia(fmt::format("[{}] {}", c, msg)), codigo(c) {}
};

void demo_raiz_propria() {
    fmt::print("\n── 3.3 Uma raiz por domínio ──\n");

    const std::vector<std::function<void()>> operacoes{
        [] { throw ErroDeParse("token inesperado na linha 42"); },
        [] { throw ErroDeRede(503, "serviço indisponível"); },
        [] { throw std::bad_alloc(); },
    };

    for (const auto& op : operacoes) {
        try {
            op();
        }
        catch (const ErroDeRede& e) {
            fmt::print("      [rede, código {}] {}\n", e.codigo, e.what());
        }
        catch (const ErroDaAcademia& e) {
            fmt::print("      [nosso, genérico] {}\n", e.what());
        }
        catch (const std::exception& e) {
            fmt::print("      [do sistema]      {}\n", e.what());
        }
    }

    fmt::print("      A raiz intermediária separa \"erro nosso\" de \"erro do\n");
    fmt::print("      sistema\" sem precisar listar todos os tipos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::nested_exception — preservando a cadeia de causas
//
//     Um erro de baixo nível ("arquivo não encontrado") não diz nada ao
//     usuário. Um erro de alto nível ("falha ao iniciar o servidor") não diz
//     nada ao desenvolvedor. `std::throw_with_nested` guarda os dois.
// ─────────────────────────────────────────────────────────────────────────────
void ler_arquivo(const std::string& caminho) {
    throw std::runtime_error(fmt::format("arquivo '{}' não encontrado", caminho));
}

void carregar_certificado() {
    try {
        ler_arquivo("/etc/ssl/cert.pem");
    } catch (...) {
        // Empacota a exceção atual DENTRO da nova
        std::throw_with_nested(
            std::runtime_error("não foi possível carregar o certificado TLS"));
    }
}

void iniciar_servidor() {
    try {
        carregar_certificado();
    } catch (...) {
        std::throw_with_nested(
            std::runtime_error("falha ao iniciar o servidor na porta 443"));
    }
}

// Desempacota recursivamente, imprimindo cada nível
void imprimir_cadeia(const std::exception& e, int nivel = 0) {
    fmt::print("      {:>{}}{}\n", "", nivel * 2, e.what());
    try {
        std::rethrow_if_nested(e);          // relança a causa, se houver
    } catch (const std::exception& causa) {
        imprimir_cadeia(causa, nivel + 1);
    } catch (...) {
        fmt::print("      {:>{}}(causa de tipo desconhecido)\n", "", (nivel+1) * 2);
    }
}

void demo_nested() {
    fmt::print("\n── 3.4 std::nested_exception — cadeia de causas ──\n");

    try {
        iniciar_servidor();
    } catch (const std::exception& e) {
        fmt::print("      cadeia completa, do topo para a raiz:\n");
        imprimir_cadeia(e);
    }

    fmt::print("      Cada camada acrescenta CONTEXTO sem apagar a causa.\n");
    fmt::print("      É o equivalente ao \"caused by\" do stack trace de outras\n");
    fmt::print("      linguagens — só que explícito.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O que NÃO fazer
// ─────────────────────────────────────────────────────────────────────────────
void demo_anti_padroes() {
    fmt::print("\n── 3.5 Anti-padrões ──\n");

    fmt::print("      ✗ throw 42;                    → lance tipos, não valores nus\n");
    fmt::print("      ✗ throw new MinhaExcecao();    → vaza; lance por valor\n");
    fmt::print("      ✗ catch (MinhaExcecao e)       → corta; capture por const&\n");
    fmt::print("      ✗ catch (...) {{ }}              → engolir silenciosamente\n");
    fmt::print("      ✗ throw e;  (para repassar)    → use `throw;`\n");
    fmt::print("      ✗ exceção que escapa do dtor   → std::terminate\n");
    fmt::print("      ✗ exceção que escapa de thread → std::terminate\n");
    fmt::print("      ✗ exceção no caminho comum     → use um valor de retorno\n");
    fmt::print("                                       (ver demo_expected.hpp)\n");

    fmt::print("\n      Sobre `catch(...)`: é legítimo EXATAMENTE em três lugares —\n");
    fmt::print("        1. no `main`, para virar código de saída\n");
    fmt::print("        2. na função de uma thread, para repassar via exception_ptr\n");
    fmt::print("        3. numa fronteira com C, para não deixar escapar\n");
    fmt::print("      Em qualquer outro lugar, você está escondendo um bug.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_logic_vs_runtime();
    demo_customizada();
    demo_raiz_propria();
    demo_nested();
    demo_anti_padroes();
}

} // namespace demo_hierarchy
