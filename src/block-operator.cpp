#include <cstring>
#include <vector>

#include "block-operator.hpp"
#include "macros/assert.hpp"

namespace buse {
namespace {
template <class T, class U>
auto align(const T value, const U unit) -> T {
    return (value / unit) * unit;
}

auto generic_io(BlockOperator& self, const bool write, const size_t offset, const size_t len, void* const buf) -> int {
    constexpr auto error_value = EIO;

    if(len % self.block_size == 0 && offset % self.block_size == 0) {
        const auto blocks = len / self.block_size;
        const auto block  = offset / self.block_size;
        ensure_v(write ? self.write_block(block, blocks, buf) : self.read_block(block, blocks, buf));
        return 0;
    }

    const auto aligned_offset = align(offset, self.block_size);
    const auto offset_gap     = offset - aligned_offset;
    const auto block          = aligned_offset / self.block_size;
    const auto blocks         = ((len + offset_gap) + self.block_size - 1) / self.block_size;
    auto       tmp            = std::vector<std::byte>(blocks * self.block_size);
    if(write) {
        if(offset_gap != 0) {
            // read leading reminder
            ensure_v(self.read_block(block, 1, tmp.data()));
        }
        const auto len_exclude_leading_remainder = len - (self.block_size - offset_gap);
        if(len_exclude_leading_remainder % self.block_size != 0) {
            // read trailing reminder
            const auto last_block     = block + blocks - 1;
            const auto last_block_buf = tmp.data() + (blocks - 1) * self.block_size;
            ensure_v(self.read_block(last_block, 1, last_block_buf));
        }
        // compose supplied buffer and temporary buffer
        memcpy(tmp.data() + offset_gap, buf, len);
        // writeback
        ensure_v(self.write_block(block, blocks, tmp.data()));
    } else {
        // read whole sectors
        ensure_v(self.read_block(block, blocks, tmp.data()));
        // copy only requested region
        memcpy(buf, tmp.data() + offset_gap, len);
    }
    return 0;
}
} // namespace

auto BlockOperator::read(size_t offset, size_t len, void* buf) -> int {
    return generic_io(*this, false, offset, len, buf);
}

auto BlockOperator::write(size_t offset, size_t len, const void* buf) -> int {
    return generic_io(*this, true, offset, len, const_cast<void*>(buf));
}
} // namespace buse
