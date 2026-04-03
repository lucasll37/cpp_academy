#include "tcp_states.hpp"
#include "tcp_connection.hpp"
#include <fmt/core.h>
#include <memory>

namespace patterns {

// ── CLOSED ───────────────────────────────────────────────────────────────────
// única operação válida: open() → vai para LISTEN
void ClosedState::open(TcpConnection& conn) {
    fmt::println("  [CLOSED] abrindo socket — enviando SYN passivo");
    conn.transition_to(std::make_unique<ListenState>());
}

// ── LISTEN ───────────────────────────────────────────────────────────────────
// acknowledge() → recebeu SYN do cliente → vai para SYN_RECEIVED
// close()       → desiste de escutar → volta para CLOSED
void ListenState::acknowledge(TcpConnection& conn) {
    fmt::println("  [LISTEN] SYN recebido — enviando SYN-ACK");
    conn.transition_to(std::make_unique<SynReceivedState>());
}

void ListenState::close(TcpConnection& conn) {
    fmt::println("  [LISTEN] fechando — nenhuma conexão aceita");
    conn.transition_to(std::make_unique<ClosedState>());
}

// ── SYN_RECEIVED ─────────────────────────────────────────────────────────────
// acknowledge() → recebeu ACK do cliente → handshake completo → ESTABLISHED
// close()       → timeout ou RST → volta para CLOSED
void SynReceivedState::acknowledge(TcpConnection& conn) {
    fmt::println("  [SYN_RECEIVED] ACK recebido — handshake completo");
    conn.transition_to(std::make_unique<EstablishedState>());
}

void SynReceivedState::close(TcpConnection& conn) {
    fmt::println("  [SYN_RECEIVED] RST ou timeout — abortando");
    conn.transition_to(std::make_unique<ClosedState>());
}

// ── ESTABLISHED ──────────────────────────────────────────────────────────────
// close() → FIN enviado → volta para CLOSED (simplificado: omitimos FIN_WAIT)
void EstablishedState::close(TcpConnection& conn) {
    fmt::println("  [ESTABLISHED] enviando FIN — encerrando conexão");
    conn.transition_to(std::make_unique<ClosedState>());
}

} // namespace patterns
