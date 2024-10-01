#pragma once
#include <cstddef>

#include <errno.h>

namespace buse {
struct Operator {
    size_t block_count = 0;
    size_t block_size  = 0;

    virtual auto read(size_t /*offset*/, size_t /*len*/, void* /*buf*/) -> int {
        return EPERM;
    }

    virtual auto write(size_t /*offset*/, size_t /*len*/, const void* /*buf*/) -> int {
        return EPERM;
    }

    virtual auto disconnect() -> int {
        return 0;
    }

    virtual auto flush() -> int {
        return 0;
    }

    virtual auto trim(size_t /*from*/, size_t /*len*/) -> int {
        return 0;
    }

    virtual ~Operator() {}
};
} // namespace buse
