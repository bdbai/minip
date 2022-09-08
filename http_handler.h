//
// Created by bdbai on 22-8-14.
//

#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "conn.h"
#include "context.h"
#include "err.h"
#include "http_header.h"
#include "res_writer.h"

#ifndef MINIP_HTTP_HANDLER_H
#define MINIP_HTTP_HANDLER_H

namespace minip {

    constexpr size_t MAX_HEADER_SIZE = 1024;

    std::optional<http_req_header> try_parse_header(std::span<const uint8_t> header_buf);

    template<class C> requires res_writer_factory<C>
    struct http_handler;

    template<class C> requires res_writer_factory<C>
    struct http_task final {
        enum class http_task_state {
            INIT,
            AWAITING_HEADER,
            NEXT_HANDLER,
            DONE,
        };

        using res_writer_t = decltype(std::declval<C>()(std::declval<http_req_header const &>()));

        explicit http_task(conn conn, C &factory) : client_conn(std::move(conn)), factory(factory) {}

        bool advance(context &ctx) {
            while (true) {
                switch (state) {
                    case http_task_state::INIT:
                        header_buf = std::make_unique_for_overwrite<uint8_t[]>(MAX_HEADER_SIZE);
                        state = http_task_state::AWAITING_HEADER;
                        [[fallthrough]];
                    case http_task_state::AWAITING_HEADER: {
                        if (header_buf_read == MAX_HEADER_SIZE) {
                            throw http_err(431, "req too large");
                        }
                        auto read = ctx.read(client_conn,
                                             std::span<uint8_t>(header_buf.get() + header_buf_read,
                                                                MAX_HEADER_SIZE - header_buf_read));
                        if (!read.has_value()) {
                            return false;
                        }
                        if (read.value() == 0) {
                            state = http_task_state::DONE;
                            return true;
                        }
                        header_buf_read += read.value();
                        auto const header = try_parse_header(std::span<uint8_t>(header_buf.get(), header_buf_read));
                        if (!header.has_value()) {
                            continue;
                        }
                        header_buf_read = 0;
                        state = http_task_state::NEXT_HANDLER;
                        res_writer.template emplace<1>(factory(header.value()));
                    }
                        [[fallthrough]];
                    case http_task_state::NEXT_HANDLER: {
                        bool done;
                        if (auto p = std::get_if<1>(&res_writer)) {
                            done = p->advance(ctx, client_conn);
                        } else {
                            done = std::get<2>(res_writer).advance(ctx, client_conn);
                        }
                        if (done) {
                            res_writer = {};
                            state = http_task_state::AWAITING_HEADER;
                        } else {
                            return false;
                        }
                    }
                        break;
                    case http_task_state::DONE:
                        return true;
                }
            }
        }

        friend struct http_handler<C>;

    private:
        http_task_state state{http_task_state::INIT};
        std::unique_ptr<uint8_t[]> header_buf;
        size_t header_buf_read{};
        conn client_conn;
        C &factory;
        std::variant<std::monostate, res_writer_t, dyn_res_writer> res_writer;
    };

    template<class C> requires res_writer_factory<C>
    struct http_handler final {
        explicit http_handler(C factory) : factory(std::move(factory)) {}

        void run_task(context &ctx) noexcept {
            bool finished;
            auto task = static_cast<http_task<C> *>(ctx.get_task());
            try {
                while (true) {
                    try {
                        finished = task->advance(ctx);
                        break;
                    } catch (http_err const &e) {
                        task->res_writer.template emplace<2>(
                                buffer_res_writer(e.code, "text/plain", e.msg).into_dyn());
                    }
                }
            } catch (io_err const &e) {
                // std::cerr << e.what() << std::endl;
                finished = true;
            }
            if (finished && task->client_conn) {
                ctx.shutdown(task->client_conn);
                delete task;
            }
        }

        void on_conn(context &ctx, conn new_conn) noexcept {
            ctx.set_task(new http_task<C>(std::move(new_conn), factory));
            run_task(ctx);
        }

    private:
        C factory;
    };

} // minip

#endif //MINIP_HTTP_HANDLER_H
