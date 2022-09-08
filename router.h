//
// Created by bdbai on 22-8-14.
//

#include <optional>
#include <string>

#include "context.h"
#include "http_handler.h"
#include "http_header.h"
#include "res_writer.h"

#ifndef MINIP_ROUTER_H
#define MINIP_ROUTER_H

namespace minip {
    template<class PR, class CR>
    struct nested_router;

    template<class H> requires res_writer_factory<H>
    struct path_router {
        using res_writer_t = decltype(std::declval<H>()(std::declval<http_req_header const &>()));

        template<class H2>
        auto mount(std::string &&next_path, H2 &&next_handler) noexcept {
            return nested_router<path_router<H>, path_router<H2>>{
                    .prev_router = std::move(*this),
                    .curr_router = path_router<H2>{
                            .path = std::move(next_path),
                            .handler = std::forward<H2>(next_handler),
                    }
            };
        }

        std::optional<res_writer_t> execute(http_req_header const &header) {
            if (header.url != path) {
                return std::nullopt;
            }
            return std::make_optional(handler(header));
        }

        std::string path;
        H handler;
    };

    template<class PR, class CR>
    struct nested_router {
        template<class H>
        auto mount(std::string &&path, H &&handler) noexcept {
            return nested_router<nested_router<PR, CR>, path_router<H>>{
                    .prev_router = std::move(*this),
                    .curr_router = path_router<H>{
                            .path = std::move(path),
                            .handler = std::forward<H>(handler),
                    }
            };
        }

        std::optional<dyn_res_writer> execute(http_req_header const &header) {
            auto prev_res = prev_router.execute(header);
            if (prev_res.has_value()) {
                return std::make_optional(prev_res.value().into_dyn());
            }
            auto curr_res = curr_router.execute(header);
            if (curr_res.has_value()) {
                return std::make_optional(curr_res.value().into_dyn());
            }
            return std::nullopt;
        }

        PR prev_router;
        CR curr_router;
    };

    struct noop_router {
        template<class H>
        auto mount(std::string &&path, H &&handler) const noexcept {
            return path_router<H>{
                    .path = std::move(path),
                    .handler = std::forward<H>(handler),
            };
        }
    };

    noop_router create_router();

    template<class R>
    auto create_http_handler_from_router(R route) {
        return http_handler([route = std::move(route)](auto const &req) mutable {
            auto res = route.execute(req);
            if (res.has_value()) {
                return std::move(res.value());
            }
            throw http_err(404, "Cannot find path");
        });
    }

} // minip

#endif //MINIP_ROUTER_H
