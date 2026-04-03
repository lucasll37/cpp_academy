#pragma once
#include "tcp_state.hpp"
#include <memory>

namespace patterns {

struct TcpConnection {
    TcpConnection();

    void open();
    void close();
    void acknowledge();

    std::string state_name() const;

    // chamado pelos estados para trocar o estado corrente
    void transition_to(std::unique_ptr<TcpState> next);

private:
    std::unique_ptr<TcpState> state_;
};

} // namespace patterns
