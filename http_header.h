//
// Created by bdbai on 22-8-14.
//

#include <map>
#include <span>
#include <string>
#include <optional>

#ifndef MINIP_HTTP_HEADER_H
#define MINIP_HTTP_HEADER_H

namespace minip {
    struct http_req_header {
        std::string_view method;
        std::string url;
        std::multimap<std::string, std::string> header;
        size_t body_at;
    };

    struct http_res_header {
        [[nodiscard]] std::string_view get_code_desc() const noexcept;

        [[nodiscard]] size_t estimate_header_length() const noexcept;

        [[nodiscard]] std::optional<size_t> serialize(std::span<uint8_t> buf) const noexcept;

        uint16_t code{};
        std::string_view content_type;
        size_t content_length{};
        std::multimap<std::string, std::string> header;
    };
} // minip

#endif //MINIP_HTTP_HEADER_H
