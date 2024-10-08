#include <array>
#include <cstring>
#include <iostream>
#include <vector>

#include <fcntl.h>
#include <linux/nbd.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "macros/assert.hpp"
#include "operator.hpp"

namespace {
/*
 * These helper functions were taken from cliserv.h in the nbd distribution.
 */
#ifdef WORDS_BIGENDIAN
auto ntohll(const uint64_t a) -> uint64_t {
    return a;
}
#else
auto ntohll(const uint64_t a) -> uint64_t {
    auto lo = a & 0xffffffff;
    auto hi = a >> 32U;
    lo      = ntohl(lo);
    hi      = ntohl(hi);
    return ((uint64_t)lo) << 32U | hi;
}
#endif
#define htonll ntohll
} // namespace

namespace buse {
namespace {
using SigAction  = struct sigaction;
using NBDRequest = struct nbd_request;
using NBDReply   = struct nbd_reply;

template <class... Args>
auto print(Args... args) -> void {
    (std::cout << ... << args) << std::endl;
}
} // namespace

auto read_all(const int fd, std::byte* buf, size_t count) -> int {
    while(count > 0) {
        const auto bytes_read = read(fd, buf, count);
        ensure(bytes_read > 0);
        buf += bytes_read;
        count -= bytes_read;
    }
    ensure(count == 0);
    return 0;
}

auto write_all(const int fd, const std::byte* buf, size_t count) -> int {
    while(count > 0) {
        const auto bytes_written = write(fd, buf, count);
        ensure(bytes_written > 0);
        buf += bytes_written;
        count -= bytes_written;
    }
    ensure(count == 0);
    return 0;
}

/* Signal handler to gracefully disconnect from nbd kernel driver. */
auto dev_to_disconnect = -1;
auto disconnect_nbd(const int /*signal*/) -> void {
    if(dev_to_disconnect != -1) {
        return;
    }

    if(ioctl(dev_to_disconnect, NBD_DISCONNECT) != -1) {
        print("sucessfuly requested disconnect on nbd device");
        dev_to_disconnect = -1;
    } else {
        print("failed to request disconect on nbd device");
    }
}

/* Sets signal action like regular sigaction but is suspicious. */
auto set_sigaction(const int sig, const struct sigaction* const act) -> int {
    auto       oact = SigAction{};
    const auto r    = sigaction(sig, act, &oact);
    if(r == 0 && oact.sa_handler != SIG_DFL) {
        printf("overriden non-default signal handler (%d: %s)", sig, strsignal(sig));
    }
    return r;
}

/* Serve userland side of nbd socket. If everything worked ok, return 0. */
auto serve_nbd(const int socket, Operator& op) -> int {
    auto request = NBDRequest{};
    auto reply   = NBDReply{
          .magic  = htonl(NBD_REPLY_MAGIC),
          .error  = htonl(0),
          .cookie = 0,
    };

loop:
    const auto bytes_read = read(socket, &request, sizeof(request));
    ensure(bytes_read == sizeof(request), bytes_read, " != ", sizeof(request));
    memcpy(reply.handle, request.handle, sizeof(reply.handle));
    reply.error     = htonl(0);
    const auto len  = ntohl(request.len);
    const auto from = ntohll(request.from);
    ensure(request.magic == htonl(NBD_REQUEST_MAGIC));

    switch(ntohl(request.type)) {
        /* I may at some point need to deal with the the fact that the
         * official nbd server has a maximum buffer size, and divides up
         * oversized requests into multiple pieces. This applies to reads
         * and writes.
         */
    case NBD_CMD_READ: {
        auto buf    = std::vector<std::byte>(len);
        reply.error = htonl(op.read(from, len, buf.data()));
        write_all(socket, std::bit_cast<std::byte*>(&reply), sizeof(struct nbd_reply));
        write_all(socket, buf.data(), len);
    } break;
    case NBD_CMD_WRITE: {
        auto buf = std::vector<std::byte>(len);
        read_all(socket, buf.data(), len);
        reply.error = htonl(op.write(from, len, buf.data()));
        write_all(socket, std::bit_cast<std::byte*>(&reply), sizeof(struct nbd_reply));
    } break;
    case NBD_CMD_DISC: {
        op.disconnect();
        return 0;
    } break;
    case NBD_CMD_FLUSH: {
        reply.error = htonl(op.flush());
        write_all(socket, std::bit_cast<std::byte*>(&reply), sizeof(struct nbd_reply));
    } break;
    case NBD_CMD_TRIM: {
        reply.error = htonl(op.trim(from, len));
        write_all(socket, std::bit_cast<std::byte*>(&reply), sizeof(struct nbd_reply));
    } break;
    default:
        return 1;
    }
    goto loop;
}

auto run(const char* const nbd_path, Operator& op) -> int {
    auto socket = std::array<int, 2>();
    ensure(socketpair(AF_UNIX, SOCK_STREAM, 0, socket.data()) == 0);
    const auto nbd = open(nbd_path, O_RDWR);
    ensure(nbd > 0);

    // ensure(ioctl(nbd, NBD_SET_SIZE, op.file_size) != -1);
    ensure(ioctl(nbd, NBD_SET_BLKSIZE, op.block_size) != -1);
    ensure(ioctl(nbd, NBD_SET_SIZE_BLOCKS, op.block_count) != -1);
    ensure(ioctl(nbd, NBD_CLEAR_SOCK) != -1);

    const auto pid = fork();
    if(pid == 0) {
        /* Block all signals to not get interrupted in ioctl(NBD_DO_IT), as
         * it seems there is no good way to handle such interruption.*/
        auto sigset = sigset_t();
        ensure(sigfillset(&sigset) == 0);
        ensure(sigprocmask(SIG_SETMASK, &sigset, NULL) == 0);
        close(socket[0]);
        ensure(ioctl(nbd, NBD_SET_SOCK, socket[1]) != -1);
        ensure(ioctl(nbd, NBD_SET_FLAGS, NBD_FLAG_SEND_TRIM | NBD_FLAG_SEND_FLUSH) != -1);
        ensure(ioctl(nbd, NBD_DO_IT) != -1);
        ensure(ioctl(nbd, NBD_CLEAR_QUE) != -1);
        ensure(ioctl(nbd, NBD_CLEAR_SOCK) != -1);
        exit(0);
    }
    ensure(dev_to_disconnect == -1);
    dev_to_disconnect = nbd;

    auto act       = SigAction{};
    act.sa_handler = disconnect_nbd;
    act.sa_flags   = SA_RESTART;
    ensure(sigemptyset(&act.sa_mask) == 0);
    ensure(sigaddset(&act.sa_mask, SIGINT) == 0);
    ensure(sigaddset(&act.sa_mask, SIGTERM) == 0);
    ensure(set_sigaction(SIGINT, &act) == 0);
    ensure(set_sigaction(SIGTERM, &act) == 0);
    close(socket[1]);

    /* serve NBD socket */
    auto status = serve_nbd(socket[0], op);
    if(close(socket[0]) != 0) {
        print("problem closing server side nbd socket");
    }
    ensure(status == 0, status);
    if(status != 0) {
        return status;
    }

    /* wait for subprocess */
    ensure(waitpid(pid, &status, 0) != -1);
    ensure(WEXITSTATUS(status) == 0, WEXITSTATUS(status));

    return 0;
}
} // namespace buse
