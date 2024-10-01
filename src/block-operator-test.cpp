#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "block-operator.hpp"
#include "macros/assert.hpp"

struct Operator : buse::BlockOperator {
    std::string buffer;

    auto read_block(size_t block, size_t blocks, void* buf) -> bool override {
        memcpy(buf, &buffer[block_size * block], block_size * blocks);
        return true;
    }

    auto write_block(size_t block, size_t blocks, const void* buf) -> bool override {
        memcpy(&buffer[block_size * block], buf, block_size * blocks);
        return true;
    }

    Operator(const size_t block_size, const size_t block_count) {
        this->block_size  = block_size;
        this->block_count = block_count;
        this->buffer      = std::string(block_size * block_count, '.');
    }
};

auto test() -> bool {
    auto op = Operator(8, 8);
    ensure(op.buffer == "........"
                        "........"
                        "........"
                        "........"
                        "........"
                        "........"
                        "........"
                        "........");

    ensure(op.write(0, 8, "11111111") == 0);
    ensure(op.buffer == "11111111"
                        "........"
                        "........"
                        "........"
                        "........"
                        "........"
                        "........"
                        "........");

    ensure(op.write(12, 8, "22222222") == 0);
    ensure(op.buffer == "11111111"
                        "....2222"
                        "2222...."
                        "........"
                        "........"
                        "........"
                        "........"
                        "........");

    ensure(op.write(20, 16, "33333333"
                            "33333333") == 0);
    ensure(op.buffer == "11111111"
                        "....2222"
                        "22223333"
                        "33333333"
                        "3333...."
                        "........"
                        "........"
                        "........");

    ensure(op.write(25, 4, "4444") == 0);
    ensure(op.buffer == "11111111"
                        "....2222"
                        "22223333"
                        "34444333"
                        "3333...."
                        "........"
                        "........"
                        "........");

    ensure(op.write(32, 9, "555555555") == 0);
    ensure(op.buffer == "11111111"
                        "....2222"
                        "22223333"
                        "34444333"
                        "55555555"
                        "5......."
                        "........"
                        "........");

    ensure(op.write(39, 9, "666666666") == 0);
    ensure(op.buffer == "11111111"
                        "....2222"
                        "22223333"
                        "34444333"
                        "55555556"
                        "66666666"
                        "........"
                        "........");

    ensure(op.write(47, 17, "77777777777777777") == 0);
    ensure(op.buffer == "11111111"
                        "....2222"
                        "22223333"
                        "34444333"
                        "55555556"
                        "66666667"
                        "77777777"
                        "77777777");
    return true;
}

auto main() -> int {
    ensure(test());
    print("ok");
    return 0;
}
