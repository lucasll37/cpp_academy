#include "sort_context.hpp"

namespace patterns {

SortContext::SortContext(std::unique_ptr<Sorter> strategy)
    : strategy_(std::move(strategy)) {}

void SortContext::set_strategy(std::unique_ptr<Sorter> strategy) {
    strategy_ = std::move(strategy);
}

void SortContext::execute(std::vector<int>& data) {
    strategy_->sort(data);
}

const char* SortContext::strategy_name() const {
    return strategy_->name();
}

} // namespace patterns