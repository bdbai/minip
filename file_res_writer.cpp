//
// Created by bdbai on 22-8-17.
//

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "file_res_writer.h"

namespace minip {
    void mmap_deleter::operator()(uint8_t *p) const noexcept {
        if (p) {
            munmap(p, length);
        }
    }

    file_res_writer create_file_res_writer(std::string_view path, std::string_view mime_type) {
        auto fd = open(path.data(), O_RDONLY);
        if (fd == -1) {
            auto const err = std::string(strerror(errno));
            throw std::runtime_error(err);
        }

        struct stat stat_buf{};
        if (fstat(fd, &stat_buf) == -1) {
            close(fd);
            auto const err = std::string(strerror(errno));
            throw std::runtime_error(err);
        }
        auto file_size = static_cast<size_t>( stat_buf.st_size);
        http_res_header const header{
                .code = 200,
                .content_type = mime_type,
                .content_length = file_size,
                .header = {{"Connection", "Keep-Alive"}},
        };
        auto header_buf_size = header.estimate_header_length();

        if (file_size < SMALL_FILE_THRESHOLD) {
            header_buf_size += file_size;
            auto buf = std::make_shared_for_overwrite<uint8_t[]>(header_buf_size);
            auto header_size = header.serialize({buf.get(), buf.get() + header_buf_size}).value();
            if (read(fd, buf.get() + header_size, file_size) != static_cast<ssize_t>(file_size)) {
                throw std::runtime_error("Error while reading file");
            }
            close(fd);
            return {std::move(buf), header_size + file_size, {}, 0};
        }

        auto mmap_raw_ptr = static_cast<uint8_t *>(mmap(nullptr, file_size, PROT_READ,
                                                        MAP_PRIVATE, fd, 0));
        close(fd);
        if (mmap_raw_ptr == (void *) -1) {
            auto const err = std::string(strerror(errno));
            throw std::runtime_error(err);
        }
        std::shared_ptr<uint8_t> mmap_ptr(mmap_raw_ptr, mmap_deleter{.length = file_size});
        auto buf = std::make_shared_for_overwrite<uint8_t[]>(header_buf_size);
        auto header_size = header.serialize({buf.get(), buf.get() + header_buf_size}).value();
        return {std::move(buf), header_size, std::move(mmap_ptr), file_size};
    }

    bool file_res_writer::advance(context &ctx, conn &client_conn) {
        while (true) {
            if (written < header_len) {
                auto to_write = std::min(header_len - written, MAX_WRITE_SIZE);
                auto const res = ctx.write(client_conn, {header.get() + written, header.get() + written + to_write});
                if (!res.has_value()) {
                    return false;
                }
                written += res.value();
                continue;
            }
            if (!mmap_file) {
                return true;
            }
            auto offset = written - header_len;
            auto to_write = std::min(mmap_file_len - offset, MAX_WRITE_SIZE);
            if (to_write == 0) {
                return true;
            }
            auto const res = ctx.write(client_conn, {mmap_file.get() + offset, mmap_file.get() + offset + to_write});
            if (!res.has_value()) {
                return false;
            }
            written += res.value();
        }
    }

} // minip