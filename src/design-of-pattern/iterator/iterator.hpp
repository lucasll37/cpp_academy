#pragma once

namespace patterns {

template<typename T>
struct Iterator {
    virtual ~Iterator() = default;
    virtual bool  has_next() const = 0;
    virtual T     next()           = 0;
};

} // namespace patterns
