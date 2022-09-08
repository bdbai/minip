//
// Created by bdbai on 22-8-14.
//

#include "worker.h"

namespace minip {
    void worker_set::on_conn(minip::conn new_conn) noexcept {
        // Simple round-rubin selection
        auto selected_worker_id = (last_selected_worker + 1) % workers.size();
        auto &worker = workers[selected_worker_id];
        last_selected_worker = selected_worker_id;

        {
            std::lock_guard _g{worker->new_fd_queue_lock};
            worker->new_fd_queue.emplace_back(std::move(new_conn));
        }
        if (eventfd_write(worker->notify_efd, 1) == -1) {
            std::cerr << "Cannot write notify efd" << std::endl;
        }
    }
} // minip
