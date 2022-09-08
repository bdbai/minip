//
// Created by bdbai on 22-8-14.
//

#include <cstdint>
#include <span>

#ifdef WITH_URING

#include <liburing.h>

#endif // WITH_URING

#ifndef MINIP_CONN_H
#define MINIP_CONN_H

namespace minip {
    constexpr uint8_t WANT_READ = 0b00000001;
    constexpr uint8_t WANT_WRITE = 0b00000010;

    struct conn final {
        static conn from_fd(int fd) noexcept;

        conn(conn const &) = delete;

        conn(conn &&) noexcept;

        conn &operator=(conn &&) = delete;

        conn &operator=(conn const &) = delete;

        explicit operator bool() const noexcept;

        [[nodiscard]] int get_raw_fd() const noexcept;

        void set_nonblock() const;

        ~conn();

        uint8_t interest{};
#ifdef WITH_URING
        std::span<uint8_t> inflight_read_buf{};
        std::span<uint8_t const> inflight_write_buf{};
        io_uring_sqe *pending_sqe{};
#endif // WITH_URING
    private:
        explicit conn(int fd) noexcept: fd(fd) {}

        int fd;
    };
} // minip

#endif //MINIP_CONN_H
