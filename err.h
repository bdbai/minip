//
// Created by bdbai on 22-8-14.
//

#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

#ifndef MINIP_ERR_H
#define MINIP_ERR_H

namespace minip {
    struct io_err : std::exception {
        io_err();

        explicit io_err(int custom_errno) : err_msg(std::strerror(custom_errno)) {}

        [[nodiscard]] char const *what() const noexcept override;

    private:
        char const *err_msg;
    };

    struct http_err : std::exception {
        http_err(uint16_t code, std::string msg) : code(code), msg(std::move(msg)) {}

        [[nodiscard]] char const *what() const noexcept override;

        uint16_t code;
        std::string msg;
    };

} // minip

#endif //MINIP_ERR_H
