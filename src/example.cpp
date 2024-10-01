#include <vector>

#include "block-operator.hpp"
#include "buse.hpp"
#include "util/argument-parser.hpp"

struct Operator : buse::Operator {
    std::vector<std::byte> buffer;

    auto read(const size_t offset, const size_t len, void* const buf) -> int override {
        printf("read: 0x%08lx %lu\n", offset, len);
        memcpy(buf, &buffer[offset], len);
        return 0;
    }

    auto write(const size_t offset, const size_t len, const void* const buf) -> int override {
        printf("write: 0x%08lx %lu\n", offset, len);
        memcpy(&buffer[offset], buf, len);
        return 0;
    }

    Operator(const size_t block_size, const size_t block_count) {
        this->block_size  = block_size;
        this->block_count = block_count;
        this->buffer.resize(block_size * block_count);
    }
};

struct BlockOperator : buse::BlockOperator {
    std::vector<std::byte> buffer;

    auto read_block(size_t block, size_t blocks, void* buf) -> bool override {
        printf("read: 0x%08lx %lu\n", block_size * block, block_size * blocks);
        memcpy(buf, &buffer[block_size * block], block_size * blocks);
        return true;
    }

    auto write_block(size_t block, size_t blocks, const void* buf) -> bool override {
        printf("write: 0x%08lx %lu\n", block_size * block, block_size * blocks);
        memcpy(&buffer[block_size * block], buf, block_size * blocks);
        return true;
    }

    BlockOperator(const size_t block_size, const size_t block_count) {
        this->block_size  = block_size;
        this->block_count = block_count;
        this->buffer.resize(block_size * block_count);
    }
};

auto main(const int argc, const char* const* argv) -> int {
    auto block_size  = 512;
    auto block_count = 4096;
    auto use_block   = false;
    auto nbd_dev     = "/dev/nbd0";
    auto parser      = args::Parser();
    auto help        = false;
    parser.kwarg(&block_size, {"-s", "--size"}, {"BYTES", "sector size", args::State::DefaultValue});
    parser.kwarg(&block_count, {"-c", "--count"}, {"COUNT", "total sectors", args::State::DefaultValue});
    parser.kwarg(&use_block, {"-b", "--block"}, {"", "use block-based io", args::State::Initialized});
    parser.kwarg(&nbd_dev, {"-d", "--device"}, {"DEVICE_PATH", "path to nbd device file", args::State::DefaultValue});
    parser.kwarg(&help, {"-h", "--help"}, {"", "print this help message", args::State::Initialized});
    if(!parser.parse(argc, argv) || help) {
        print("usage: nbd-example ", parser.get_help());
        return 0;
    }

    auto op = (buse::Operator*)(nullptr);
    if(use_block) {
        op = new BlockOperator(block_size, block_count);
    } else {
        op = new Operator(block_size, block_count);
    }
    return buse::run(nbd_dev, *op);
}
