//
// Created by bdbai on 22-8-14.
//

#include "http_handler.h"

#include <algorithm>
#include <map>
#include <optional>

#include "err.h"

namespace minip {

    std::optional<http_req_header> try_parse_header(std::span<const uint8_t> header_buf) {
        auto double_newline_ptr = memmem(header_buf.data(), header_buf.size(), "\r\n\r\n", 4);
        if (double_newline_ptr == nullptr) {
            return std::nullopt;
        }
        size_t header_size =
                reinterpret_cast<uintptr_t>(double_newline_ptr) - reinterpret_cast<uintptr_t >(header_buf.data()) + 2;
        if (std::any_of(header_buf.data(), header_buf.data() + header_size, [](auto b) {
            return b > 127;
        })) {
            throw http_err(400, "bad req header");
        }

        auto first_return_ptr = memmem(header_buf.data(), header_size, "\r\n", 2);
        if (first_return_ptr == nullptr) {
            throw http_err(400, "bad req header");
        }
        size_t status_line_size =
                reinterpret_cast<uintptr_t>(first_return_ptr) - reinterpret_cast<uintptr_t>(header_buf.data());
        auto first_space_ptr = memchr(header_buf.data(), ' ', status_line_size);
        if (first_space_ptr == nullptr) {
            throw http_err(400, "bad req header");
        }
        size_t method_size =
                reinterpret_cast<uintptr_t>(first_space_ptr) - reinterpret_cast<uintptr_t>(header_buf.data());
        std::string_view method;
        if (method_size == 3 && memcmp(header_buf.data(), "GET ", 4) == 0) {
            method = "GET";
        } else if (method_size == 4 && memcmp(header_buf.data(), "POST ", 5) == 0) {
            method = "POST";
        } else if (method_size == 3 && memcmp(header_buf.data(), "PUT ", 4) == 0) {
            method = "PUT";
        } else if (method_size == 6 && memcmp(header_buf.data(), "DELETE ", 7) == 0) {
            method = "DELETE";
        } else {
            throw http_err(400, "bad req header");
        }

        auto second_space_ptr = memchr(
                static_cast<uint8_t const *>(first_space_ptr) + 1,
                ' ',
                status_line_size - method_size - 1);
        if (second_space_ptr == nullptr) {
            return std::nullopt;
        }
        size_t url_len =
                reinterpret_cast<uintptr_t>(second_space_ptr) - reinterpret_cast<uintptr_t>(first_space_ptr) - 1;
        std::string url(reinterpret_cast<char const *>(first_space_ptr) + 1, url_len);

        std::multimap<std::string, std::string> headers;
        // TODO: parse headers

        return http_req_header{
                .method = method,
                .url = std::move(url),
                .header = headers,
                .body_at = header_size + 4,
        };
    }
} // minip
