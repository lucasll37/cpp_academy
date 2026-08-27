#include "quick_sort.hpp"

namespace patterns {

void QuickSort::sort(std::vector<int>& data) {
    if (data.size() < 2) return;
    quick(data, 0, static_cast<int>(data.size()) - 1);
}

void QuickSort::quick(std::vector<int>& data, int lo, int hi) {
    if (lo >= hi) return;
    int p = partition(data, lo, hi);
    quick(data, lo, p - 1);
    quick(data, p + 1, hi);
}

int QuickSort::partition(std::vector<int>& data, int lo, int hi) {
    int pivot = data[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; ++j)
        if (data[j] <= pivot)
            std::swap(data[++i], data[j]);
    std::swap(data[i + 1], data[hi]);
    return i + 1;
}

const char* QuickSort::name() const { return "QuickSort"; }

} // namespace patterns