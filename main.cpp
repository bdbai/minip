#include <atomic>
#include <thread>

#include "file_res_writer.h"
#include "http_handler.h"
#include "res_writer.h"
#include "router.h"
#include "serve.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Too few arguments. Listen address and port required." << std::endl;
        return 1;
    }
    auto listen_addr = argv[1];
    auto listen_port = argv[2];

    auto index_file = minip::create_file_res_writer("./static/index.html", "text/html");
    auto video_file = minip::create_file_res_writer("./static/video.mp4", "video/mp4");
    std::atomic_int32_t visitor_count{};
    auto router = minip::create_router()
            .mount("/", [=](auto const &) {
                return index_file;
            })
            .mount("/visitor_count", [&](auto const &) {
                auto current_count = visitor_count.fetch_add(1, std::memory_order_relaxed) + 1;
                return minip::buffer_res_writer(200, "text/plain", std::to_string(current_count));
            })
            .mount("/index.html", [=](auto const &) {
                return index_file;
            })
            .mount("/video.mp4", [=](auto const &) {
                return video_file;
            });

    minip::worker_set worker_set(std::thread::hardware_concurrency(),
                                 minip::create_http_handler_from_router(std::move(router)));
    if (minip::serve(listen_addr, listen_port, worker_set)) {
        return 0;
    } else {
        return 2;
    }
}
