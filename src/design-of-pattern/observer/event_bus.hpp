#pragma once
#include "observer.hpp"
#include <vector>

namespace patterns {

struct EventBus {
    void subscribe(Observer* observer);
    void unsubscribe(Observer* observer);
    void publish(const Event& event);
private:
    std::vector<Observer*> observers_;
};

} // namespace patterns