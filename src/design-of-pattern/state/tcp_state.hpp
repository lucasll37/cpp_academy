#pragma once
#include <string>

namespace patterns {

struct TcpConnection;

struct TcpState {
    virtual ~TcpState() = default;

    virtual void open        (TcpConnection& conn);
    virtual void close       (TcpConnection& conn);
    virtual void acknowledge (TcpConnection& conn);

    virtual std::string name() const = 0;
};

} // namespace patterns
