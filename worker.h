//
// Created by bdbai on 22-8-14.
//

#ifndef MINIP_WORKER_H
#define MINIP_WORKER_H

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <thread>
#include <utility>
#include <vector>

#ifdef WITH_URING

#include <liburing.h>

#endif // WITH_URING

#include "conn.h"
#include "context.h"

namespace minip {
    constexpr unsigned int MAX_EVENT = 1024;

    struct worker_set final {
        struct worker {
            int notify_efd;
            std::mutex new_fd_queue_lock;
            std::vector<conn> new_fd_queue;
        };

        template<class H>
        explicit worker_set(unsigned int worker_count, H const &handler) {
            if (worker_count == 0) {
                throw std::invalid_argument("Worker count cannot be zero");
            }

            int efd_flag = EFD_NONBLOCK;
#ifdef WITH_URING
            bool uring_supported = false;
            io_uring probe_ring {};
            if (io_uring_queue_init(1024, &probe_ring, 0) == 0) {
                auto uring_probe = io_uring_get_probe();
                uring_supported = uring_probe != nullptr;
                io_uring_free_probe(uring_probe);
            } else {
                std::cerr << "Cannot initialize an io_uring queue. Either kernel version is too low, or "
                             "maximum size allowed to lock into memory is too low?" << std::endl
                          << "To use io_uring, please run as root and try again." << std::endl;
            }

            if (uring_supported) {
                efd_flag = 0;
                std::cerr << "Using io_uring" << std::endl;
            } else {
                std::cerr << "Using epoll" << std::endl;
            }
#else
            std::cerr << "io_uring support is not available. Please rebuilt with package liburing installed to use"
                             "io_uring." << std::endl;
#endif // WITH_URING

            workers.reserve(static_cast<size_t>(worker_count));
            for (unsigned int i = 0; i < worker_count; i++) {
                auto worker = std::make_shared<worker_set::worker>();
                auto efd = eventfd(0, efd_flag);
                if (efd == -1) {
                    throw std::runtime_error("Cannot create event fd");
                }
                worker->notify_efd = efd;
                workers.emplace_back(worker);
#ifdef WITH_URING
                if (uring_supported) {
                    std::thread worker_thread(uring_worker_func<H>, std::move(worker), handler);
                    worker_thread.detach();
                } else {
                    std::thread worker_thread(epoll_worker_func<H> , std::move(worker), handler);
                    worker_thread.detach();
                }
#else
                std::thread worker_thread(epoll_worker_func<H> , std::move(worker), handler);
                worker_thread.detach();
#endif // WITH_URING
            }
        }

        worker_set(worker_set const &) = delete;

        worker_set &operator=(worker_set &) = delete;

        void on_conn(conn new_conn) noexcept;

    private:
#ifdef WITH_URING

        template<class H>
        static void uring_worker_func(std::shared_ptr<worker_set::worker> w, H handler) {
            struct io_uring ring{};
            if (auto res{io_uring_queue_init(1024, &ring, 0)}; res != 0) {
                throw std::runtime_error("Cannot initialize uring queue");
            }
            std::array<uint8_t, 8> notify_ev_buf{};
            bool notify_efd_need_submit = true;
            std::vector<void *> backlog_tasks;
            backlog_tasks.reserve(MAX_EVENT);

            std::vector<conn> new_fd_queue;
            io_uring_sqe *notify_sqe {};
            while (true) {
                new_fd_queue.clear();
                do {
                    if (notify_efd_need_submit) {
                        if (notify_sqe == nullptr) {
                            io_uring_sqring_wait(&ring);
                            notify_sqe = io_uring_get_sqe(&ring);
                            if (notify_sqe == nullptr) {
                                // throw std::runtime_error("Cannot get notify sqe");
                                break;
                            }
                            io_uring_prep_read(notify_sqe, w->notify_efd, notify_ev_buf.data(), notify_ev_buf.size(),
                                               0);
                            io_uring_sqe_set_data(notify_sqe, nullptr);
                        }
                        auto const res{io_uring_submit(&ring)};
                        if (res >= 0) {
                            notify_sqe = nullptr;
                            notify_efd_need_submit = false;
                        }
                    }
                } while (false);

                io_uring_cqe *cqe;
                if (io_uring_wait_cqe(&ring, &cqe) != 0) {
                    std::cerr << "Cannot wait cqe: " << strerror(errno) << std::endl;
                    continue;
                }

                if (cqe->user_data == 0) {
                    notify_efd_need_submit = true;
                } else {
                    context ctx(&ring, io_uring_cqe_get_data(cqe));
                    ctx.cqe = cqe;
                    handler.run_task(ctx);
                    if (ctx.retry_needed) {
                        backlog_tasks.push_back(ctx.task);
                    }
                }
                io_uring_cqe_seen(&ring, cqe);

                while (!backlog_tasks.empty()) {
                    context ctx(&ring, backlog_tasks[0]);
                    handler.run_task(ctx);
                    if (ctx.retry_needed) {
                        break;
                    } else {
                        backlog_tasks.erase(backlog_tasks.begin());
                    }
                }

                {
                    std::unique_lock _g{w->new_fd_queue_lock};
                    std::swap(w->new_fd_queue, new_fd_queue);
                }
                for (auto &c: new_fd_queue) {
                    context ctx(&ring, nullptr);
                    handler.on_conn(ctx, std::move(c));
                    if (ctx.retry_needed) {
                        backlog_tasks.push_back(ctx.task);
                    }
                }
            }
            io_uring_queue_exit(&ring);
        }

#endif // WITH_URING

        template<class H>
        static void epoll_worker_func(std::shared_ptr<worker_set::worker> w, H handler) {
            int res;
            auto epfd = epoll_create1(0);
            auto add_notify_efd_event = epoll_event{
                    .events = EPOLLIN | EPOLLET,
                    .data = {0}
            };
            res = epoll_ctl(epfd, EPOLL_CTL_ADD, w->notify_efd, &add_notify_efd_event);
            if (res == -1) {
                std::cerr << "Cannot add notify eventfd to epoll" << std::endl;
                return;
            }

            std::vector<epoll_event> events(MAX_EVENT);

            std::vector<conn> new_fd_queue;
            while (true) {
                events.clear();
                new_fd_queue.clear();
                if (new_fd_queue.capacity() > MAX_EVENT) {
                    new_fd_queue.shrink_to_fit();
                }

                res = epoll_wait(epfd, events.data(), static_cast<int>(events.capacity()), -1);
                if (res == -1) {
                    std::cerr << "Cannot epoll_wait: " << strerror(errno) << std::endl;
                }
                context ctx(epfd, nullptr);
                for (int i = 0; i < res; i++) {
                    auto evptr = events[i].data.ptr;
                    if (evptr == nullptr) {
                        continue;
                    }
                    ctx.task = evptr;
                    handler.run_task(ctx);
                }
                {
                    std::unique_lock _g{w->new_fd_queue_lock};
                    std::swap(w->new_fd_queue, new_fd_queue);
                }
                for (auto &c: new_fd_queue) {
                    ctx.task = nullptr;
                    c.set_nonblock();
                    handler.on_conn(ctx, std::move(c));
                }
            }
        }

        std::vector<std::shared_ptr<worker>> workers;
        size_t last_selected_worker{};
    };
} // minip


#endif //MINIP_WORKER_H
