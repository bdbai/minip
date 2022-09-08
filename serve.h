//
// Created by bdbai on 22-8-14.
//

#ifndef MINIP_SERVE_H
#define MINIP_SERVE_H

#include <cstdint>
#include <string>

#include "worker.h"

namespace minip {
    bool serve(char const *listen_addr, char const *listen_port, worker_set &set);
} // minip


#endif //MINIP_SERVE_H
