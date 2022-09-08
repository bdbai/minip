# minip

C++ Mini Project: basic Linux HTTP server

## Features

1. Handle HTTP/1.1 requests (with connection keepalive)
2. I/O Multiplexing using epoll or io_uring
3. Serves local files (using mmap) or text data
4. Fluent API to chain route handlers

## Example

```c++
#include <atomic>
#include <thread>

#include "file_res_writer.h"
#include "http_handler.h"
#include "res_writer.h"
#include "router.h"
#include "serve.h"

int main() {
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
    minip::serve("127.0.0.1", "8220", worker_set);
}
```

### Build and Run
```shell
# Optional: install liburing for io_uring support
sudo pacman -S liburing

# Build
cmake -DCMAKE_BUILD_TYPE=Release . && make

# Lift restrictions for open files/connections
ulimit -n unliminted

# Run server
./minip 127.0.0.1 8220

# Note that the limit for locked memory is too low for our server to create an
# IO ring. In this case, simply run as root.
sudo ./minip 127.0.0.1 8220
```

## Runtime model
minip uses a multi-thread model to handle connections in parallel. The runtime consists of a main thread, which will
receive new connections and block the main function, and a few worker threads. For each incoming connection, the
ownership is transferred to one of the worker threads where HTTP request parsing and response composing happen. Each
worker thread has its own event loop, backed by io_uring or epoll. An incoming connection will always be handled by the
same thread.

## Performance
Tested on my laptop with Intel(R) Core(TM) i7-7700HQ CPU @ 2.80GHz (8 logical cores) and 8 GB memory.

### epoll, light load
```shell
 wrk -t 8 -c64 -d5s http://127.0.0.1:8220/
Running 5s test @ http://127.0.0.1:8220/
  8 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   318.39us  831.04us  17.11ms   94.64%
    Req/Sec    48.23k     4.32k   85.08k    82.22%
  1943528 requests in 5.10s, 0.92GB read
Requests/sec: 381091.77
Transfer/sec:    183.90MB
```

### epoll, heavy load
```shell
Running 5s test @ http://127.0.0.1:8220/
  8 threads and 10000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    22.26ms   11.63ms  99.56ms   71.13%
    Req/Sec    30.47k     8.99k   63.23k    78.69%
  1159593 requests in 5.09s, 559.57MB read
Requests/sec: 228015.12
Transfer/sec:    110.03MB
```

### io_uring, light load
```shell
❯ wrk -t 8 -c64 -d5s http://127.0.0.1:8220/
Running 5s test @ http://127.0.0.1:8220/
  8 threads and 64 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   550.64us    1.81ms  29.58ms   94.83%
    Req/Sec    50.62k     5.50k  105.40k    74.56%
  2024685 requests in 5.10s, 0.95GB read
Requests/sec: 397092.13
Transfer/sec:    191.62MB
```
### io_uring, heavy load
```shell
❯ wrk -t 8 -c10000 -d5s http://127.0.0.1:8220/
Running 5s test @ http://127.0.0.1:8220/
  8 threads and 10000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    24.42ms   11.70ms 103.05ms   73.13%
    Req/Sec    28.03k     6.68k   57.04k    81.56%
  1058213 requests in 5.08s, 510.65MB read
Requests/sec: 208211.03
Transfer/sec:    100.47MB
```
