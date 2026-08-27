#pragma once
#include "sorter.hpp"

namespace patterns {

struct BubbleSort : Sorter {
    void sort(std::vector<int>& data) override;
    const char* name() const override;
};

} // namespace patterns