#include "tcp_connection.hpp"
#include <fmt/core.h>

using namespace patterns;

static void separator(const char* label) {
    fmt::println("\n=== {} ===", label);
}

int main() {
    TcpConnection conn;

    // ── cenário 1: handshake completo ─────────────────────────────────────
    separator("handshake completo");
    fmt::println("estado inicial: {}", conn.state_name());
    conn.open();          // CLOSED → LISTEN
    conn.acknowledge();   // LISTEN → SYN_RECEIVED
    conn.acknowledge();   // SYN_RECEIVED → ESTABLISHED
    fmt::println("estado final:   {}", conn.state_name());

    // ── cenário 2: encerramento normal ────────────────────────────────────
    separator("encerramento");
    conn.close();         // ESTABLISHED → CLOSED
    fmt::println("estado final:   {}", conn.state_name());

    // ── cenário 3: eventos inválidos ──────────────────────────────────────
    separator("eventos inválidos (operações ignoradas)");
    conn.acknowledge();   // CLOSED não sabe o que fazer com ACK
    conn.close();         // CLOSED não sabe o que fazer com close
    fmt::println("estado final:   {}", conn.state_name());

    // ── cenário 4: RST durante handshake ─────────────────────────────────
    separator("abort durante handshake");
    conn.open();          // CLOSED → LISTEN
    conn.acknowledge();   // LISTEN → SYN_RECEIVED
    conn.close();         // SYN_RECEIVED → CLOSED (RST/timeout)
    fmt::println("estado final:   {}", conn.state_name());

    return 0;
}
