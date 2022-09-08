//
// Created by bdbai on 22-8-14.
//

#include "conn.h"

#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace minip {
    conn conn::from_fd(int fd) noexcept {
        return conn(fd);
    }

    void conn::set_nonblock() const {
        auto fd_flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
        fcntl(fd, F_SETFL, fd_flags);
    }

    conn::~conn() {
        if (fd != -1) {
            close(std::exchange(fd, -1));
        }
    }

    conn::conn(conn &&that) noexcept:
            fd(std::exchange(that.fd, -1)) {}

    int conn::get_raw_fd() const noexcept {
        return fd;
    }

    conn::operator bool() const noexcept {
        return fd != -1;
    }
} // minip