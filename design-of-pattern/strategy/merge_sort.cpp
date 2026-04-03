#include "merge_sort.hpp"
#include <vector>

namespace patterns {

void MergeSort::sort(std::vector<int>& data) {
    if (data.size() < 2) return;
    merge_sort(data, 0, static_cast<int>(data.size()) - 1);
}

void MergeSort::merge_sort(std::vector<int>& data, int lo, int hi) {
    if (lo >= hi) return;
    int mid = lo + (hi - lo) / 2;
    merge_sort(data, lo, mid);
    merge_sort(data, mid + 1, hi);
    merge(data, lo, mid, hi);
}

void MergeSort::merge(std::vector<int>& data, int lo, int mid, int hi) {
    std::vector<int> left(data.begin() + lo, data.begin() + mid + 1);
    std::vector<int> right(data.begin() + mid + 1, data.begin() + hi + 1);
    size_t i = 0, j = 0;
    int k = lo;
    while (i < left.size() && j < right.size())
        data[k++] = (left[i] <= right[j]) ? left[i++] : right[j++];
    while (i < left.size())  data[k++] = left[i++];
    while (j < right.size()) data[k++] = right[j++];
}

const char* MergeSort::name() const { return "MergeSort"; }

} // namespace patterns