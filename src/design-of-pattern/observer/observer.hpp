#pragma once
#include <string>

namespace patterns {

struct Event {
    std::string source;
    std::string message;
};

struct Observer {
    virtual ~Observer() = default;
    virtual void on_event(const Event& event) = 0;
};

} // namespace patterns