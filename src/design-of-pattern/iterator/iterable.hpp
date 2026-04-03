#pragma once
#include "iterator.hpp"
#include <memory>

namespace patterns {

template<typename T>
struct Iterable {
    virtual ~Iterable() = default;
    virtual std::unique_ptr<Iterator<T>> forward_iterator()  const = 0;
    virtual std::unique_ptr<Iterator<T>> reverse_iterator()  const = 0;
};

} // namespace patterns
