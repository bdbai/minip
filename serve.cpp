//
// Created by bdbai on 22-8-14.
//

#include "serve.h"

#include <cstring>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "conn.h"

namespace minip {
    constexpr int LISTEN_BACKLOG = 1024;

    static int bind_to_addr(char const *listen_addr, char const *listen_port) {
        int res;

        struct addrinfo hints, *addrinfo_res, *rp;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_canonname = nullptr;
        hints.ai_addr = nullptr;
        hints.ai_next = nullptr;
        res = getaddrinfo(listen_addr, listen_port, &hints, &addrinfo_res);
        if (res != 0) {
            std::cerr << "Cannot resolve listen addr" << std::endl;
            return -1;
        }
        if (addrinfo_res == nullptr) {
            std::cerr << "No listen addr resolved" << std::endl;
            return -1;
        }

        int listen_fd = -1;
        for (rp = addrinfo_res; rp != nullptr; rp = rp->ai_next) {
            listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listen_fd == -1) {
                continue;
            }

            int reuseaddr_opt = 1;
            res = setsockopt(
                    listen_fd,
                    SOL_SOCKET,
                    SO_REUSEADDR, &reuseaddr_opt,
                    sizeof(reuseaddr_opt));
            if (res != 0) {
                std::cerr << "Cannot set reuseaddr" << std::endl;
                close(listen_fd);
                return -1;
            }

            res = bind(listen_fd, rp->ai_addr, rp->ai_addrlen);
            if (res == 0) {
                break;
            }

            close(listen_fd);
            listen_fd = -1;
        }
        freeaddrinfo(addrinfo_res);
        if (rp == nullptr) {
            std::cerr << "Cannot bind to an address" << std::endl;
            return -1;
        }
        return listen_fd;
    }

    bool serve(char const *listen_addr, char const *listen_port, worker_set &set) {
        auto listen_fd = bind_to_addr(listen_addr, listen_port);
        if (listen_fd != -1) {
            std::cerr << "Serving at " << listen_addr << ":" << listen_port << std::endl;
        }

        int s;
        s = listen(listen_fd, LISTEN_BACKLOG);
        if (s != 0) {
            std::cerr << "Cannot listen on server socket" << std::endl;
            return false;
        }

        while (true) {
            int conn_fd = accept(listen_fd, nullptr, nullptr);
            if (conn_fd == -1) {
                std::cerr << "Cannot accept: " << strerror(errno) << std::endl;
                continue;
            }
            set.on_conn(conn::from_fd(conn_fd));
        }
    }
} // minip
