#include "event_bus.hpp"
#include <algorithm>

namespace patterns {

void EventBus::subscribe(Observer* observer) {
    observers_.push_back(observer);
}

void EventBus::unsubscribe(Observer* observer) {
    observers_.erase(
        std::remove(observers_.begin(), observers_.end(), observer),
        observers_.end()
    );
}

void EventBus::publish(const Event& event) {
    for (auto* obs : observers_) {
        obs->on_event(event);
    }
}

} // namespace patterns