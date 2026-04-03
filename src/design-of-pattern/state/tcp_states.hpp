#pragma once
#include "tcp_state.hpp"

namespace patterns {

struct ClosedState : TcpState {
    void open(TcpConnection& conn) override;
    std::string name() const override { return "CLOSED"; }
};

struct ListenState : TcpState {
    void acknowledge(TcpConnection& conn) override;
    void close      (TcpConnection& conn) override;
    std::string name() const override { return "LISTEN"; }
};

struct SynReceivedState : TcpState {
    void acknowledge(TcpConnection& conn) override;
    void close      (TcpConnection& conn) override;
    std::string name() const override { return "SYN_RECEIVED"; }
};

struct EstablishedState : TcpState {
    void close(TcpConnection& conn) override;
    std::string name() const override { return "ESTABLISHED"; }
};

} // namespace patterns
