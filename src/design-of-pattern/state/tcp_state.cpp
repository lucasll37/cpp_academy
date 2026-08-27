#include "tcp_state.hpp"
#include "tcp_connection.hpp"
#include <fmt/core.h>

namespace patterns {

void TcpState::open(TcpConnection&) {
    fmt::println("  [{}] open() ignorado neste estado", name());
}

void TcpState::close(TcpConnection&) {
    fmt::println("  [{}] close() ignorado neste estado", name());
}

void TcpState::acknowledge(TcpConnection&) {
    fmt::println("  [{}] acknowledge() ignorado neste estado", name());
}

} // namespace patterns
