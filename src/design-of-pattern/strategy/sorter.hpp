#pragma once
#include <vector>

namespace patterns {

struct Sorter {
    virtual ~Sorter() = default;
    virtual void sort(std::vector<int>& data) = 0;
    virtual const char* name() const = 0;
};

} // namespace patterns