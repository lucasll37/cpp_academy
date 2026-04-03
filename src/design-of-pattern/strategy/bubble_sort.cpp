#include "bubble_sort.hpp"

namespace patterns {

void BubbleSort::sort(std::vector<int>& data) {
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j + 1 < data.size() - i; ++j)
            if (data[j] > data[j + 1])
                std::swap(data[j], data[j + 1]);
}

const char* BubbleSort::name() const { return "BubbleSort"; }

} // namespace patterns