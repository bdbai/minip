//
// Created by bdbai on 22-8-15.
//

#include <algorithm>
#include <memory>
#include <vector>

#include "context.h"
#include "http_header.h"

#ifndef MINIP_RES_WRITER_H
#define MINIP_RES_WRITER_H

namespace minip {
    constexpr size_t MAX_WRITE_SIZE = 8192;

    struct dyn_res_writer_inner_base {
        virtual bool advance(context &ctx, conn &client_conn) = 0;

        virtual ~dyn_res_writer_inner_base() = default;
    };

    template<class R>
    struct enable_into_dyn_res_writer;

    struct dyn_res_writer {
        bool advance(context &ctx, conn &client_conn) const {
            return r->advance(ctx, client_conn);
        }

        dyn_res_writer into_dyn() {
            return dyn_res_writer(std::move(r));
        }

        template<class R>
        friend
        struct enable_into_dyn_res_writer;
    private:
        explicit dyn_res_writer(std::unique_ptr<dyn_res_writer_inner_base> r)
                : r(std::move(r)) {}

        std::unique_ptr<dyn_res_writer_inner_base> r;
    };

    template<class R>
    struct enable_into_dyn_res_writer {
        struct dyn_res_writer_wrapper : public dyn_res_writer_inner_base {
            explicit dyn_res_writer_wrapper(R r) : r(std::move(r)) {}

            bool advance(context &ctx, conn &client_conn) override {
                return r.advance(ctx, client_conn);
            }

        private:
            R r;
        };

        dyn_res_writer into_dyn() {
            return dyn_res_writer(std::make_unique<dyn_res_writer_wrapper>(std::move(*static_cast<R *>(this))));
        }
    };

    template<class C>
    concept buffer_container =
    std::is_pointer_v<decltype(std::declval<C>().data())>
    && std::same_as<decltype(std::declval<C>().size()), size_t>;

    struct buffer_res_writer : public enable_into_dyn_res_writer<buffer_res_writer> {
        template<class B>
        requires buffer_container<B>
        buffer_res_writer(uint16_t code, std::string_view content_type, B data) {
            http_res_header const h{
                    .code = code,
                    .content_type = content_type,
                    .content_length = data.size(),
                    .header = {{"Connection", "Keep-Alive"}},
            };
            auto const buf_len = h.estimate_header_length() + data.size();
            buffer = std::make_unique_for_overwrite<uint8_t[]>(buf_len);
            auto const serialize_res = h.serialize({buffer.get(), buf_len});
            size_t const header_len = serialize_res.value();
            total = header_len + data.size();
            std::copy(data.data(), data.data() + data.size(), buffer.get() + header_len);
        }

        bool advance(context &ctx, conn &client_conn);

    private:
        std::unique_ptr<uint8_t[]> buffer;
        size_t total{};
        size_t offset{};
    };

    template<class C, class R = decltype(std::declval<C>()(
            std::declval<http_req_header const &>()))>
    concept res_writer_factory =
    std::same_as<decltype(std::declval<R &>().advance(std::declval<context &>(), std::declval<conn &>())), bool>
    && std::copy_constructible<C>;
} // minip

#endif //MINIP_RES_WRITER_H
