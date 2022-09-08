//
// Created by bdbai on 22-8-14.
//

#include "err.h"

#include <cstring>
#include <cerrno>

namespace minip {
    io_err::io_err() : err_msg(strerror(errno)) {}

    char const *io_err::what() const noexcept {
        return err_msg;
    }

    char const *http_err::what() const noexcept {
        return msg.c_str();
    }
} // minip