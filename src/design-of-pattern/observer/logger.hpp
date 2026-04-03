#pragma once
#include "observer.hpp"

namespace patterns {

struct Logger : Observer {
    void on_event(const Event& event) override;
};

} // namespace patterns