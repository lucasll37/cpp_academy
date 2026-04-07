// =============================================================================
//  src/network/demo_timer.hpp
//
//  Timers com Asio standalone
//  ───────────────────────────
//  Asio integra timers ao mesmo event loop de I/O.
//  Sem threads extras, sem sleep() bloqueante.
//  O timer expira e o handler é chamado pelo io_context — exatamente
//  como um handler de socket.
//
//  Tópicos:
//  • steady_timer síncrono (wait)
//  • steady_timer assíncrono (async_wait)
//  • Timer cancelável
//  • Timer periódico (heartbeat)
//  • work_guard — mantém io_context vivo sem trabalho pendente
// =============================================================================
#pragma once

#define ASIO_STANDALONE
#include <asio.hpp>

#include <fmt/core.h>
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <vector>

namespace demo_timer {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. steady_timer síncrono — wait() bloqueia até expirar
// ─────────────────────────────────────────────────────────────────────────────
void demo_sincrono() {
    fmt::print("\n── 4.1 steady_timer síncrono ──\n");

    asio::io_context io;

    // steady_timer: baseado em relógio monotônico (não afetado por ajuste de hora)
    asio::steady_timer timer(io, 30ms);

    fmt::print("  aguardando 30ms...\n");
    timer.wait(); // bloqueia até expirar
    fmt::print("  timer expirou\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. steady_timer assíncrono — async_wait com handler
//     async_wait retorna imediatamente — handler chamado pelo io_context
// ─────────────────────────────────────────────────────────────────────────────
void demo_assincrono() {
    fmt::print("\n── 4.2 steady_timer assíncrono ──\n");

    asio::io_context io;
    asio::steady_timer timer(io, 20ms);

    timer.async_wait([](asio::error_code ec) {
        if (!ec)
            fmt::print("  handler: timer expirou!\n");
        else
            fmt::print("  handler: {}\n", ec.message());
    });

    fmt::print("  async_wait retornou imediatamente\n");
    fmt::print("  chamando io_context.run()...\n");
    io.run(); // aqui o handler é chamado
    fmt::print("  io_context.run() retornou — nenhum trabalho pendente\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Timer cancelável
//     cancel() faz o handler ser chamado com error_code = operation_aborted
// ─────────────────────────────────────────────────────────────────────────────
void demo_cancelamento() {
    fmt::print("\n── 4.3 Timer cancelável ──\n");

    asio::io_context io;
    asio::steady_timer timer(io, 1000ms); // expiraria em 1 segundo

    timer.async_wait([](asio::error_code ec) {
        if (ec == asio::error::operation_aborted)
            fmt::print("  timer cancelado antes de expirar ✓\n");
        else if (!ec)
            fmt::print("  timer expirou (não deveria)\n");
    });

    timer.cancel(); // cancela antes de io.run()
    io.run();
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Timer periódico — heartbeat / tick
//     Padrão: no handler, reconfigura e re-registra async_wait
// ─────────────────────────────────────────────────────────────────────────────
void demo_periodico() {
    fmt::print("\n── 4.4 Timer periódico ──\n");

    asio::io_context io;
    asio::steady_timer timer(io);
    int ticks = 0;
    constexpr int MAX = 4;

    // std::function necessário para o handler se referenciar recursivamente
    std::function<void(asio::error_code)> tick;
    tick = [&](asio::error_code ec) {
        if (ec) return;
        fmt::print("  tick #{}\n", ++ticks);
        if (ticks < MAX) {
            timer.expires_after(15ms); // reconfigura
            timer.async_wait(tick);    // re-registra
        }
        // quando ticks == MAX: não re-registra → io.run() retorna
    };

    timer.expires_after(15ms);
    timer.async_wait(tick);
    io.run();

    fmt::print("  {} ticks executados\n", ticks);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. work_guard — mantém io_context.run() ativo sem trabalho pendente
//
//  Problema: io_context.run() retorna quando não há mais trabalho.
//  Se você quer um pool de threads sempre pronto, precisa do work_guard.
//  Ele diz ao io_context: "ainda há trabalho — não retorne do run()".
// ─────────────────────────────────────────────────────────────────────────────
void demo_work_guard() {
    fmt::print("\n── 4.5 io_context multithreaded com work_guard ──\n");

    asio::io_context io;
    auto guard = asio::make_work_guard(io); // mantém io.run() ativo

    std::atomic<int> executadas{0};

    // Pool de 3 threads todas rodando o mesmo io_context
    std::vector<std::thread> pool;
    for (int i = 0; i < 3; ++i)
        pool.emplace_back([&io]() { io.run(); });

    // Posta 6 tarefas — distribuídas entre as 3 threads
    for (int i = 0; i < 6; ++i) {
        asio::post(io, [i, &executadas]() {
            fmt::print("  tarefa {} executada\n", i);
            ++executadas;
        });
    }

    std::this_thread::sleep_for(50ms);
    guard.reset(); // libera work_guard → io.run() pode retornar

    for (auto& t : pool) t.join();
    fmt::print("  {} tarefas executadas por 3 threads\n", executadas.load());
}

inline void run() {
    demo_sincrono();
    demo_assincrono();
    demo_cancelamento();
    demo_periodico();
    demo_work_guard();
}

} // namespace demo_timer
