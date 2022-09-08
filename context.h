//
// Created by bdbai on 22-8-14.
//

#include <cstdint>
#include <optional>
#include <span>

#ifdef WITH_URING

#include <liburing.h>

#endif // WITH_URING

#include "conn.h"

#ifndef MINIP_CONTEXT_H
#define MINIP_CONTEXT_H

namespace minip {
    struct worker_set;

    struct context {
        std::optional<size_t> read(conn &c, std::span<uint8_t> data);

        std::optional<size_t> write(conn &c, std::span<const uint8_t> data);

        void shutdown(conn &c) const;

        [[nodiscard]] void *get_task() const noexcept;

        void set_task(void *task);

        friend minip::worker_set;
    private:
        static uint32_t interest_to_epoll(uint8_t interest) noexcept;

        context(int epfd, void *task) : epfd(epfd), task(task) {}

#ifdef WITH_URING

        context(io_uring *ring, void *task) : ring(ring), task(task) {}

        std::optional<size_t> uring_read(conn &c, std::span<uint8_t> data);

        std::optional<size_t> uring_write(conn &c, std::span<uint8_t const> data);

#endif // WITH_URING

        bool register_interest(conn &c, uint8_t new_interest) const;

        int epfd{-1};
#ifdef WITH_URING
        io_uring *ring{};
        io_uring_cqe *cqe{};
        bool retry_needed{};
#endif // WITH_URING
        void *task;
    };

} // minip

#endif //MINIP_CONTEXT_H
