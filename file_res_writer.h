//
// Created by bdbai on 22-8-17.
//

#include "res_writer.h"

#ifndef MINIP_FILE_RES_WRITER_H
#define MINIP_FILE_RES_WRITER_H

namespace minip {
    constexpr size_t SMALL_FILE_THRESHOLD = 64 * 1024;

    struct mmap_deleter {
        void operator()(uint8_t *p) const noexcept;

        size_t length{};
    };

    struct file_res_writer;

    file_res_writer create_file_res_writer(std::string_view path, std::string_view mime_type);

    struct file_res_writer : enable_into_dyn_res_writer<file_res_writer> {
        bool advance(context &ctx, conn &client_conn);

        friend file_res_writer create_file_res_writer(std::string_view path, std::string_view mime_type);

    private:
        file_res_writer(
                std::shared_ptr<uint8_t[]> header,
                size_t header_len,
                std::shared_ptr<uint8_t> mmap_file,
                size_t mmap_file_len
        ) :
                header(std::move(header)),
                header_len(header_len),
                mmap_file(std::move(mmap_file)),
                mmap_file_len(mmap_file_len) {}

        std::shared_ptr<uint8_t[]> header;
        size_t header_len;
        std::shared_ptr<uint8_t> mmap_file;
        size_t mmap_file_len;
        size_t written{};
    };

} // minip

#endif //MINIP_FILE_RES_WRITER_H
