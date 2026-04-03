#include "sort_context.hpp"
#include "bubble_sort.hpp"
#include "quick_sort.hpp"
#include "merge_sort.hpp"
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <vector>

using namespace patterns;

int main() {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};

    SortContext ctx(std::make_unique<BubbleSort>());

    std::vector<std::unique_ptr<Sorter>> strategies;
    strategies.push_back(std::make_unique<BubbleSort>());
    strategies.push_back(std::make_unique<QuickSort>());
    strategies.push_back(std::make_unique<MergeSort>());

    for (auto& strategy : strategies) {
        std::vector<int> copy = data;
        ctx.set_strategy(std::move(strategy));
        ctx.execute(copy);
        fmt::println("{}: {}", ctx.strategy_name(), copy);
    }

    return 0;
}