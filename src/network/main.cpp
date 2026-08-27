// =============================================================================
//  src/network/main.cpp
//  Ponto de entrada — executa todos os demos de rede em sequência
// =============================================================================
#include "demo_tcp_sync.hpp"
#include "demo_tcp_async.hpp"
#include "demo_udp.hpp"
#include "demo_timer.hpp"
#include "demo_http_raw.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::steel_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · TCP Síncrono — connect, read, write");
    demo_tcp_sync::run();

    section("2 · TCP Assíncrono — async_read / async_write");
    demo_tcp_async::run();

    section("3 · UDP — datagramas");
    demo_udp::run();

    section("4 · Timers — steady_timer e io_context");
    demo_timer::run();

    section("5 · HTTP sobre TCP raw");
    demo_http_raw::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de rede concluídos!\n\n");
}
