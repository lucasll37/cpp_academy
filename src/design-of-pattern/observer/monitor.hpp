#pragma once
#include "observer.hpp"
#include <vector>

namespace patterns {

struct Monitor : Observer {
    void on_event(const Event& event) override;
    const std::vector<Event>& history() const;
private:
    std::vector<Event> history_;
};

} // namespace patterns