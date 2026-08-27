// =============================================================================
//  src/errors/demo_exceptions.hpp
//
//  Exceções — o mecanismo
//  ──────────────────────
//  Uma exceção separa o CAMINHO DE ERRO do caminho normal. O código feliz não
//  precisa checar retorno a cada linha, e o erro não pode ser ignorado por
//  omissão — se ninguém tratar, o programa termina.
//
//  O que acontece num `throw`:
//    1. o objeto lançado é copiado/movido para uma área controlada pelo runtime
//    2. a pilha é DESENROLADA (stack unwinding) até o primeiro handler compatível
//    3. durante o desenrolamento, TODO destrutor de objeto local roda
//    4. o handler executa; ao terminar, o objeto da exceção é destruído
//
//  O passo 3 é o motivo de RAII e exceções serem a mesma conversa.
//
//  Tópicos:
//  • throw / try / catch e a ordem dos handlers
//  • Stack unwinding provado com destrutores
//  • Por que capturar por REFERÊNCIA
//  • Relançar com `throw;` vs `throw e;`
//  • catch(...) e std::current_exception
//  • O custo: zero-cost até lançar
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace demo_exceptions {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O básico — lançar, capturar, e a ordem dos handlers
//
//     Os handlers são testados EM ORDEM, de cima para baixo, e o primeiro
//     compatível vence. Não há "melhor match" como na resolução de overload.
//     Por isso o handler mais DERIVADO tem que vir primeiro.
// ─────────────────────────────────────────────────────────────────────────────
double dividir(int a, int b) {
    if (b == 0) {
        throw std::domain_error(fmt::format("divisão por zero: {}/{}", a, b));
    }
    return static_cast<double>(a) / b;
}

void demo_basico() {
    fmt::print("\n── 1.1 throw / try / catch ──\n");

    try {
        fmt::print("      10/2 = {}\n", dividir(10, 2));
        fmt::print("      10/0 = ");
        fmt::print("{}\n", dividir(10, 0));      // lança
        fmt::print("      esta linha nunca executa\n");
    }
    // std::domain_error deriva de std::logic_error, que deriva de std::exception.
    // Se este handler viesse DEPOIS do de std::exception, nunca seria alcançado.
    catch (const std::domain_error& e) {
        fmt::print("\n      capturado domain_error: {}\n", e.what());
    }
    catch (const std::exception& e) {
        fmt::print("      capturado exception genérico: {}\n", e.what());
    }

    fmt::print("      execução continua normalmente após o try/catch\n");
    fmt::print("      ⚠  handlers são testados EM ORDEM — do mais derivado\n");
    fmt::print("         para o mais base. O GCC avisa com -Wexceptions quando\n");
    fmt::print("         um handler fica inalcançável.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Stack unwinding — a prova
//
//     Todo objeto local construído entre o `try` e o `throw` é destruído,
//     na ordem inversa da construção. É isso que torna RAII à prova de exceção.
// ─────────────────────────────────────────────────────────────────────────────
struct Marcador {
    std::string nome;
    explicit Marcador(std::string n) : nome(std::move(n)) {
        fmt::print("        + {} construído\n", nome);
    }
    ~Marcador() { fmt::print("        - {} DESTRUÍDO (durante o unwinding)\n", nome); }
};

void nivel_3() {
    Marcador m("nível-3");
    fmt::print("        nível-3 vai lançar...\n");
    throw std::runtime_error("erro originado no nível 3");
}

void nivel_2() {
    Marcador m("nível-2");
    nivel_3();
    fmt::print("        nível-2: inalcançável\n");
}

void nivel_1() {
    Marcador m("nível-1");
    nivel_2();
    fmt::print("        nível-1: inalcançável\n");
}

void demo_unwinding() {
    fmt::print("\n── 1.2 Stack unwinding ──\n");

    try {
        nivel_1();
    } catch (const std::runtime_error& e) {
        fmt::print("      handler alcançado: {}\n", e.what());
    }

    fmt::print("      Os 3 destrutores rodaram, na ordem inversa, sem\n");
    fmt::print("      nenhuma linha de código de limpeza escrita à mão.\n");
    fmt::print("      É por isso que RAII e exceções são o mesmo assunto:\n");
    fmt::print("      se o recurso está num objeto, ele é liberado de graça.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Capturar por referência — e por que a cópia é pior
//
//     `catch (Base e)` por valor CORTA a exceção (object slicing, o mesmo
//     problema de src/oop/demo_slicing.hpp): você perde o tipo real e a
//     mensagem sobrescrita.
// ─────────────────────────────────────────────────────────────────────────────
struct ErroDeRede : std::runtime_error {
    int codigo;
    ErroDeRede(int c, const std::string& msg)
        : std::runtime_error(msg), codigo(c) {}

    // Informação que só existe no derivado
    std::string detalhe() const { return fmt::format("[HTTP {}] {}", codigo, what()); }
};

void demo_captura_por_referencia() {
    fmt::print("\n── 1.3 Capturar por referência ──\n");

    // ✗ Por valor: corta para std::runtime_error, o `codigo` some.
    //   O GCC já pega este caso com -Wcatch-value (que vem em -Wall);
    //   silenciamos porque o erro É a demonstração.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcatch-value"
    try {
        throw ErroDeRede(503, "serviço indisponível");
    } catch (std::runtime_error e) {           // ← cópia + corte
        fmt::print("      [por valor]      {} — e o código HTTP se perdeu\n", e.what());
    }
#pragma GCC diagnostic pop

    // ✓ Por referência const: nenhuma cópia, tipo real preservado
    try {
        throw ErroDeRede(503, "serviço indisponível");
    } catch (const ErroDeRede& e) {
        fmt::print("      [por referência] {}\n", e.detalhe());
    }

    fmt::print("      Regra: SEMPRE `catch (const T&)`.\n");
    fmt::print("      Lançar, por outro lado, é sempre POR VALOR: `throw T(...)`,\n");
    fmt::print("      nunca `throw new T(...)` — isso vazaria.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Relançar — `throw;` vs `throw e;`
//
//     `throw;`   relança O MESMO objeto, com o tipo dinâmico intacto
//     `throw e;` lança uma CÓPIA de `e`, cortada para o tipo estático
// ─────────────────────────────────────────────────────────────────────────────
void repassa_corretamente() {
    try {
        throw ErroDeRede(404, "não encontrado");
    } catch (const std::exception&) {
        fmt::print("        camada intermediária: logando e repassando\n");
        throw;                       // ✓ o ErroDeRede original continua sendo ErroDeRede
    }
}

void repassa_cortando() {
    try {
        throw ErroDeRede(404, "não encontrado");
    } catch (const std::exception& e) {
        fmt::print("        camada intermediária: logando e repassando\n");
        throw e;                     // ✗ lança uma cópia de std::exception
    }
}

void demo_relancar() {
    fmt::print("\n── 1.4 `throw;` vs `throw e;` ──\n");

    fmt::print("      com `throw;`\n");
    try {
        repassa_corretamente();
    } catch (const ErroDeRede& e) {
        fmt::print("      → chegou como ErroDeRede: {}\n", e.detalhe());
    } catch (const std::exception& e) {
        fmt::print("      → chegou degradado: {}\n", e.what());
    }

    fmt::print("      com `throw e;`\n");
    try {
        repassa_cortando();
    } catch (const ErroDeRede& e) {
        fmt::print("      → chegou como ErroDeRede: {}\n", e.detalhe());
    } catch (const std::exception& e) {
        fmt::print("      → chegou degradado como {}: '{}'\n",
                   "std::exception", e.what());
    }

    fmt::print("      Para repassar, use SEMPRE `throw;` sem operando.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. catch(...) e std::current_exception
//
//     `catch (...)` pega qualquer coisa, inclusive tipos que não derivam de
//     std::exception. Útil em fronteiras: main, thread, callback de C.
//     `std::current_exception()` captura a exceção ativa num ponteiro que
//     pode ser guardado e relançado depois — inclusive em outra thread.
// ─────────────────────────────────────────────────────────────────────────────
void demo_catch_all() {
    fmt::print("\n── 1.5 catch(...) e std::exception_ptr ──\n");

    // Qualquer tipo pode ser lançado — inclusive um int nu (não faça isso)
    try {
        throw 42;
    } catch (const std::exception&) {
        fmt::print("      não chega aqui: int não deriva de std::exception\n");
    } catch (...) {
        fmt::print("      catch(...) pegou algo que não é std::exception\n");
    }

    // Guardando a exceção para tratar depois
    std::exception_ptr guardada;
    try {
        throw ErroDeRede(500, "erro interno");
    } catch (...) {
        guardada = std::current_exception();   // empacota a exceção ativa
        fmt::print("      exceção guardada num std::exception_ptr\n");
    }

    fmt::print("      ...tempo depois, possivelmente em outra thread...\n");
    try {
        if (guardada) std::rethrow_exception(guardada);
    } catch (const ErroDeRede& e) {
        fmt::print("      relançada e capturada com o tipo intacto: {}\n", e.detalhe());
    }

    fmt::print("      É assim que std::future transporta a exceção de uma\n");
    fmt::print("      thread para quem chama .get() (ver src/concurrency/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. O custo — "zero-cost" até o throw
//
//     A implementação moderna (table-driven) não gasta NADA no caminho feliz:
//     não há teste, não há flag, não há registro de handler. Em troca, o
//     caminho de erro é caro: o runtime consulta tabelas para desenrolar.
//
//     Conclusão prática: exceções para o EXCEPCIONAL. Se o "erro" acontece
//     com frequência (arquivo não existe, entrada do usuário inválida),
//     um valor de retorno é melhor — ver demo_expected.hpp.
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 1.6 O custo ──\n");

    constexpr int N = 20'000;
    volatile long acc = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) acc = acc + i;                     // caminho feliz
    auto t1 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        try { if (i >= 0) throw std::runtime_error("x"); }
        catch (const std::runtime_error&) { acc = acc + i; }
    }
    auto t2 = std::chrono::steady_clock::now();

    using us = std::chrono::microseconds;
    auto feliz = std::chrono::duration_cast<us>(t1 - t0).count();
    auto erro  = std::chrono::duration_cast<us>(t2 - t1).count();

    fmt::print("      {} iterações:\n", N);
    fmt::print("        sem exceção        : {:>7} µs\n", feliz);
    fmt::print("        lançando+capturando: {:>7} µs\n", erro);
    fmt::print("      O caminho feliz não paga nada; o throw paga MUITO.\n");
    fmt::print("      Use exceções para o excepcional, não para o esperado.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_unwinding();
    demo_captura_por_referencia();
    demo_relancar();
    demo_catch_all();
    demo_custo();
}

} // namespace demo_exceptions
