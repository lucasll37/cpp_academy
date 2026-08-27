#pragma once
#include "sorter.hpp"

namespace patterns {

struct MergeSort : Sorter {
    void sort(std::vector<int>& data) override;
    const char* name() const override;
private:
    void merge_sort(std::vector<int>& data, int lo, int hi);
    void merge(std::vector<int>& data, int lo, int mid, int hi);
};

} // namespace patterns