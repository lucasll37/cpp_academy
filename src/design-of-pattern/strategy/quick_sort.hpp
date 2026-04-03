#pragma once
#include "sorter.hpp"

namespace patterns {

struct QuickSort : Sorter {
    void sort(std::vector<int>& data) override;
    const char* name() const override;
private:
    void quick(std::vector<int>& data, int lo, int hi);
    int  partition(std::vector<int>& data, int lo, int hi);
};

} // namespace patterns