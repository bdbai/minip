//
// Created by bdbai on 22-8-14.
//

#include "http_header.h"

#include <algorithm>

namespace minip {
    std::string_view http_res_header::get_code_desc() const noexcept {
        switch (code) {
            case 200:
                return "OK";
            case 400:
                return "Bad Request";
            case 404:
                return "Not Found";
            case 500:
                return "Internal Server Error";
            case 501:
                return "Not Implemented";
            default:
                return "?";
        }
    }

    size_t http_res_header::estimate_header_length() const noexcept {
        size_t const status_line_len = 9 /* HTTP/1.1  */
                                       + 50 /* 450 Blocked by Windows Parental Controls\r\n */;
        size_t headers_len = 16/* Content-Type: \r\n */
                             + content_type.size()
                             + 37 /* Content-Length: 9223372036854775807\r\n */;
        for (auto const &[k, v]: header) {
            headers_len += k.size() + v.size() + 2 /* :  */ + 2 /* \r\n */;
        }
        return status_line_len + headers_len + 2 /* \r\n */;
    }

    std::optional<size_t> http_res_header::serialize(std::span<uint8_t> buf) const noexcept {
        using namespace std::literals;

        auto *pos = buf.data();
        size_t rem = buf.size();
        auto const ver = "HTTP/1.1 "sv;
        auto const code_desc = get_code_desc();
        size_t const status_line_len = ver.size() + 4 /* 200  */ + code_desc.size();
        if (rem < status_line_len) {
            return std::nullopt;
        }
        rem -= status_line_len;
        std::copy(ver.begin(), ver.end(), pos);
        pos += ver.size();
        *(pos++) = code / 100 + '0';
        *(pos++) = (code % 100) / 10 + '0';
        *(pos++) = code % 10 + '0';
        *(pos++) = ' ';
        std::copy(code_desc.begin(), code_desc.end(), pos);
        pos += code_desc.size();

        for (auto const &[k, v]: header) {
            size_t header_line_len = 2 /* \r\n from prev line */
                                     + k.size()
                                     + 2 /* :  */
                                     + v.size();
            if (rem < header_line_len) {
                return std::nullopt;
            }
            rem -= header_line_len;
            *(pos++) = '\r';
            *(pos++) = '\n';
            std::copy(k.begin(), k.end(), pos);
            pos += k.size();
            *(pos++) = ':';
            *(pos++) = ' ';
            std::copy(v.begin(), v.end(), pos);
            pos += v.size();
        }
        auto const content_type_k = "\r\nContent-Type: "sv;
        size_t const content_type_line_len = content_type_k.size() + content_type.size();
        if (rem < content_type_line_len) {
            return std::nullopt;
        }
        std::copy(content_type_k.begin(), content_type_k.end(), pos);
        pos += content_type_k.size();
        std::copy(content_type.begin(), content_type.end(), pos);
        pos += content_type.size();
        rem -= content_type_line_len;

        auto const content_length_k = "\r\nContent-Length: "sv;
        size_t content_length_counter = content_length / 10, content_length_len = 1, content_length_multiplier = 1;
        while (content_length_counter != 0) {
            content_length_counter /= 10;
            content_length_len++;
            content_length_multiplier *= 10;
        }
        size_t const content_length_line_len = content_length_k.size() + content_length_len;
        if (rem < (content_length_line_len + 4 /* include the last CRLFs */)) {
            return std::nullopt;
        }
        std::copy(content_length_k.begin(), content_length_k.end(), pos);
        pos += content_length_k.size();
        content_length_counter = content_length;
        while (content_length_multiplier > 0) {
            auto const digit = content_length_counter / content_length_multiplier;
            *(pos++) = digit + '0';
            content_length_counter -= digit * content_length_multiplier;
            content_length_multiplier /= 10;
        }
        rem -= content_length_line_len;

        *(pos++) = '\r';
        *(pos++) = '\n';
        *(pos++) = '\r';
        *(pos++) = '\n';
        rem -= 4;

        return buf.size() - rem;
    }
} // minip