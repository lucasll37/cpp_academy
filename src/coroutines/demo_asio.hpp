// =============================================================================
//  src/coroutines/demo_asio.hpp
//
//  Demo 4 — Coroutines + Asio: I/O assíncrono de verdade
//  ────────────────────────────────────────────────────────
//  Até aqui usamos SuspendFor{} que dorme em thread separada — isso não é
//  verdadeiro async; ainda ocupa uma thread por operação.
//
//  Asio (standalone) tem suporte nativo a coroutines via:
//    • asio::awaitable<T>   — tipo de retorno para coroutines Asio
//    • co_await asio::async_read(...)  — I/O sem bloquear threads
//    • asio::co_spawn()    — lança uma coroutine no io_context
//
//  Com Asio, UMA thread pode gerenciar CENTENAS de conexões simultâneas —
//  cada conexão é uma coroutine; quando uma espera I/O, outra executa.
//
//  Tópicos:
//  • asio::awaitable<T> — o promise_type do Asio
//  • asio::use_awaitable — token que transforma callbacks em co_await
//  • asio::co_spawn() — inicia coroutine no io_context
//  • Timer assíncrono com co_await
//  • Echo server coroutine (TCP)
//  • Múltiplas coroutines concorrentes em uma thread
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <chrono>
#include <string>

namespace demo_asio {

using namespace std::chrono_literals;
using asio::ip::tcp;

// ─────────────────────────────────────────────────────────────────────────────
//  Timer assíncrono — o "hello world" de coroutines Asio
//
//  asio::awaitable<void> é como Task<void> mas implementado pelo Asio.
//  co_await timer.async_wait(asio::use_awaitable):
//    • Registra um callback no io_context
//    • Suspende a coroutine (não a thread!)
//    • Quando o timer dispara → io_context retoma a coroutine
// ─────────────────────────────────────────────────────────────────────────────
asio::awaitable<void> esperar_e_imprimir(std::string nome, std::chrono::milliseconds delay) {
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor, delay);

    fmt::print("  [{}] iniciando, esperará {}ms\n", nome, delay.count());

    // co_await suspende esta coroutine até o timer disparar
    // A thread do io_context fica LIVRE para executar outras coroutines!
    co_await timer.async_wait(asio::use_awaitable);

    fmt::print("  [{}] timer disparado após {}ms!\n", nome, delay.count());
}

void demo_timer_concorrente() {
    fmt::print("\n── 4.1 Timers concorrentes em UMA thread ──\n");
    fmt::print("  3 coroutines com delays diferentes, mesma thread:\n\n");

    asio::io_context io;

    // co_spawn lança uma coroutine no io_context
    // asio::detached = não precisamos aguardar o resultado
    asio::co_spawn(io, esperar_e_imprimir("A", 300ms), asio::detached);
    asio::co_spawn(io, esperar_e_imprimir("B", 100ms), asio::detached);
    asio::co_spawn(io, esperar_e_imprimir("C", 200ms), asio::detached);

    fmt::print("  io.run() — uma thread gerencia as 3 coroutines:\n");
    io.run(); // processa todos os eventos até não restar nenhum

    fmt::print("\n  Ordem: B(100ms) < C(200ms) < A(300ms)\n");
    fmt::print("  1 thread, 0 bloqueios — cada suspensão libera a thread\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Echo server coroutine
//
//  Cada cliente recebe uma coroutine dedicada.
//  Diferente do modelo "thread-per-connection":
//    • thread-per-connection: N clientes → N threads (cara, limita a ~1000)
//    • coroutine-per-connection: N clientes → N coroutines na mesma thread
//
//  A coroutine lê dados e os devolve ao cliente (echo).
// ─────────────────────────────────────────────────────────────────────────────
asio::awaitable<void> echo_session(tcp::socket socket) {
    try {
        char buffer[1024];
        while (true) {
            // Lê dados do cliente — suspende sem bloquear a thread
            std::size_t n = co_await socket.async_read_some(
                asio::buffer(buffer), asio::use_awaitable);

            // Devolve os mesmos dados — suspende novamente
            co_await asio::async_write(
                socket,
                asio::buffer(buffer, n),
                asio::use_awaitable);
        }
    } catch (const std::exception&) {
        // Cliente desconectou — coroutine termina naturalmente
    }
}

asio::awaitable<void> echo_listener(unsigned short porta) {
    auto executor = co_await asio::this_coro::executor;
    tcp::acceptor acceptor(executor, {tcp::v4(), porta});

    fmt::print("  [servidor] ouvindo na porta {}\n", porta);

    for (int i = 0; i < 2; ++i) { // aceita 2 conexões para o demo
        // async_accept suspende até um cliente conectar
        auto socket = co_await acceptor.async_accept(asio::use_awaitable);
        fmt::print("  [servidor] cliente {} conectado\n", i + 1);

        // Cada sessão é uma coroutine independente — não bloqueia o listener
        asio::co_spawn(executor, echo_session(std::move(socket)), asio::detached);
    }
}

asio::awaitable<void> echo_client(unsigned short porta, std::string mensagem) {
    auto executor = co_await asio::this_coro::executor;
    tcp::socket socket(executor);

    // Conecta ao servidor — suspende até conectar
    co_await socket.async_connect(
        tcp::endpoint{asio::ip::address::from_string("127.0.0.1"), porta},
        asio::use_awaitable);

    // Envia mensagem
    co_await asio::async_write(socket, asio::buffer(mensagem), asio::use_awaitable);

    // Recebe echo
    char buffer[256] = {};
    std::size_t n = co_await socket.async_read_some(
        asio::buffer(buffer), asio::use_awaitable);

    fmt::print("  [cliente] enviou: '{}' → recebeu echo: '{}'\n",
               mensagem, std::string(buffer, n));
}

void demo_echo_server() {
    fmt::print("\n── 4.2 Echo server — coroutine por conexão ──\n");

    asio::io_context io;
    const unsigned short porta = 12399;

    // Listener aceita conexões
    asio::co_spawn(io, echo_listener(porta), asio::detached);

    // Dois clientes conectam
    asio::co_spawn(io, echo_client(porta, "ola mundo!"), asio::detached);
    asio::co_spawn(io, echo_client(porta, "coroutines!"), asio::detached);

    io.run();
    fmt::print("  Servidor, listener e 2 clientes — tudo em 1 thread\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  use_awaitable vs callbacks — comparação
// ─────────────────────────────────────────────────────────────────────────────
void demo_comparacao() {
    fmt::print("\n── 4.3 Callbacks vs Coroutines — legibilidade ──\n");
    fmt::print(R"(
  Estilo callback (callback hell):
    socket.async_read(buf, [](error_code ec, size_t n) {{
        if (!ec) {{
            socket.async_write(buf, n, [](error_code ec2, size_t) {{
                if (!ec2) {{
                    socket.async_read(buf, [...] {{ /* mais nesting */ }});
                }}
            }});
        }}
    }});

  Estilo coroutine (sequencial, sem nesting):
    size_t n = co_await socket.async_read(buf, use_awaitable);
    co_await socket.async_write(buf, n, use_awaitable);
    n = co_await socket.async_read(buf, use_awaitable);

  O fluxo de controle é linear — fácil de ler, debugar e manter.
  Performance é idêntica: ambos usam o mesmo io_context por baixo.
)");
}

inline void run() {
    demo_timer_concorrente();
    demo_echo_server();
    demo_comparacao();
}

} // namespace demo_asio
