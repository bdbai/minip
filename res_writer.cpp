//
// Created by bdbai on 22-8-15.
//

#include "res_writer.h"

namespace minip {
    bool buffer_res_writer::advance(context &ctx, conn &client_conn) {
        while (offset < total) {
            size_t const to_write = std::min(total - offset, MAX_WRITE_SIZE);
            auto const write_res = ctx.write(client_conn, {buffer.get() + offset, to_write});
            if (!write_res.has_value()) {
                return false;
            }
            offset += write_res.value();
        }
        return true;
    }
} // minip