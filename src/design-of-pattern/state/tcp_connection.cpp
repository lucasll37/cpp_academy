#include "tcp_connection.hpp"
#include "tcp_states.hpp"
#include <fmt/core.h>

namespace patterns {

TcpConnection::TcpConnection()
    : state_(std::make_unique<ClosedState>()) {}

void TcpConnection::open()        { state_->open(*this);        }
void TcpConnection::close()       { state_->close(*this);       }
void TcpConnection::acknowledge() { state_->acknowledge(*this); }

std::string TcpConnection::state_name() const {
    return state_->name();
}

void TcpConnection::transition_to(std::unique_ptr<TcpState> next) {
    fmt::println("  → transição: {} → {}", state_->name(), next->name());
    state_ = std::move(next);
}

} // namespace patterns
