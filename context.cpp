//
// Created by bdbai on 22-8-14.
//

#include "context.h"

#include <cerrno>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "err.h"

namespace minip {
    std::optional<size_t> context::read(conn &c, std::span<uint8_t> data) {
#ifdef WITH_URING
        if (ring != nullptr) {
            return uring_read(c, data);
        }
#endif // WITH_URING
        auto ret = ::recv(c.get_raw_fd(), data.data(), data.size(), 0);
        if (ret >= 0) {
            register_interest(c, c.interest & (~WANT_READ));
            return {ret};
        }

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            register_interest(c, c.interest | WANT_READ);
            return std::nullopt;
        } else {
            throw io_err();
        }
    }

    std::optional<size_t> context::write(conn &c, std::span<uint8_t const> data) {
#ifdef WITH_URING
        if (ring != nullptr) {
            return uring_write(c, data);
        }
#endif // WITH_URING
        auto ret = ::send(c.get_raw_fd(), data.data(), data.size(), 0);
        if (ret >= 0) {
            register_interest(c, c.interest & (~WANT_WRITE));
            return {ret};
        }

        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            register_interest(c, c.interest | WANT_WRITE);
            return std::nullopt;
        } else {
            throw io_err();
        }
    }

    bool context::register_interest(conn &c, uint8_t new_interest) const {
        if (c.interest == new_interest) {
            return true;
        }

        auto old_interest = c.interest;
        epoll_event ev{
                .events = interest_to_epoll(old_interest),
                .data = {task},
        };
        c.interest = new_interest;

        // FIXME: ignoring epoll_ctx errors
        if (new_interest == 0) {
            epoll_ctl(epfd, EPOLL_CTL_DEL, c.get_raw_fd(), &ev);
            return true;
        }
        ev.events = interest_to_epoll(new_interest);

        if (old_interest == 0) {
            epoll_ctl(epfd, EPOLL_CTL_ADD, c.get_raw_fd(), &ev);
        } else {
            epoll_ctl(epfd, EPOLL_CTL_MOD, c.get_raw_fd(), &ev);
        }
        return true;
    }

    uint32_t context::interest_to_epoll(uint8_t interest) noexcept {
        unsigned int epev = EPOLLET;
        if (interest & WANT_READ) {
            epev |= EPOLLIN;
        }
        if (interest & WANT_WRITE) {
            epev |= EPOLLOUT;
        }
        return epev;
    }

    void context::set_task(void *task) {
        if (this->task != nullptr) {
            throw std::invalid_argument("Cannot set task because it is already set");
        }
        this->task = task;
    }

    void *context::get_task() const noexcept {
        return task;
    }

    void context::shutdown(conn &c) const {
        register_interest(c, 0);
        ::shutdown(c.get_raw_fd(), SHUT_WR);
    }

#ifdef WITH_URING

    std::optional<size_t> context::uring_read(conn &c, std::span<uint8_t> data) {
        if (auto cqe{std::exchange(this->cqe, nullptr)}) {
            auto inflight_read_buf = std::exchange(c.inflight_read_buf, {});
            if (inflight_read_buf.size() != data.size() || inflight_read_buf.data() != data.data()) {
                throw std::invalid_argument("data buffer provided is different from last read");
            }
            if (cqe->res >= 0) {
                return {cqe->res};
            } else {
                throw io_err(-cqe->res);
            }
        }

        auto sqe = std::exchange(c.pending_sqe, nullptr);
        if (sqe == nullptr) {
            io_uring_sqring_wait(ring);
            sqe = io_uring_get_sqe(ring);
            if (sqe == nullptr) {
                retry_needed = true;
                return std::nullopt;
                // throw std::runtime_error("Cannot get notify sqe");
            }
            io_uring_prep_read(sqe, c.get_raw_fd(), data.data(), data.size(), 0);
            io_uring_sqe_set_data(sqe, task);
        }
        if (auto const res{io_uring_submit(ring)}; res < 0) {
            if (res == -16) {
                retry_needed = true;
                c.pending_sqe = sqe;
                return std::nullopt;
            }
            throw std::runtime_error("Cannot submit sq while reading");
        }
        c.inflight_read_buf = data;
        return std::nullopt;
    }

    std::optional<size_t> context::uring_write(conn &c, std::span<uint8_t const> data) {
        if (auto cqe{std::exchange(this->cqe, nullptr)}) {
            auto inflight_write_buf = std::exchange(c.inflight_write_buf, {});
            if (inflight_write_buf.size() != data.size() || inflight_write_buf.data() != data.data()) {
                throw std::invalid_argument("data buffer provided is different from last write");
            }
            if (cqe->res >= 0) {
                return {cqe->res};
            } else {
                throw io_err(-cqe->res);
            }
        }

        auto sqe = std::exchange(c.pending_sqe, nullptr);
        if (sqe == nullptr) {
            io_uring_sqring_wait(ring);
            sqe = io_uring_get_sqe(ring);
            if (sqe == nullptr) {
                retry_needed = true;
                return std::nullopt;
                // throw std::runtime_error("Cannot get notify sqe");
            }
            io_uring_prep_write(sqe, c.get_raw_fd(), data.data(), data.size(), 0);
            io_uring_sqe_set_data(sqe, task);
        }
        if (auto const res{io_uring_submit(ring)}; res < 0) {
            if (res == -16) {
                retry_needed = true;
                c.pending_sqe = sqe;
                return std::nullopt;
            }
            printf("%d\r\n", res);
            throw std::runtime_error("Cannot submit sq while writing");
        }
        c.inflight_write_buf = data;
        return std::nullopt;
    }

#endif // WITH_URING
} // minip