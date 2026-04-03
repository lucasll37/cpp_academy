#pragma once
#include "sorter.hpp"
#include <vector>
#include <memory>

namespace patterns {

struct SortContext {
    explicit SortContext(std::unique_ptr<Sorter> strategy);

    void set_strategy(std::unique_ptr<Sorter> strategy);
    void execute(std::vector<int>& data);
    const char* strategy_name() const;

private:
    std::unique_ptr<Sorter> strategy_;
};

} // namespace patterns