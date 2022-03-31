#pragma once

#include <stdexcept>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "scope_guard.hpp"

namespace dfdh
{

class mmap_file_view {
public:
    inline void throw_cannot_open(const char* filename) {
        throw std::runtime_error(std::string("Cannot open file \"") + filename + "\"");
    }
    mmap_file_view() = default;

    mmap_file_view(const char* filename) {
        int fd = open(filename, O_RDONLY);
        if (fd < 0)
            throw_cannot_open(filename);
        auto scope_exit = scope_guard([&] { ::close(fd); });

        struct stat st;
        if (fstat(fd, &st) < 0)
            throw_cannot_open(filename);

        auto size = static_cast<size_t>(st.st_size);
        if (!(start = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0))))
            throw_cannot_open(filename);

        pend = start + size;
    }

    ~mmap_file_view() {
        if (start)
            ::munmap(start, static_cast<size_t>(pend - start));
    }

    mmap_file_view(mmap_file_view&& mfv) noexcept: start(mfv.start), pend(mfv.pend) {
        mfv.start = nullptr;
    }

    mmap_file_view& operator=(mmap_file_view&& mfv) noexcept {
        start     = mfv.start;
        pend      = mfv.pend;
        mfv.start = nullptr;
        return *this;
    }

    [[nodiscard]]
    const char* begin() const {
        return start;
    }

    [[nodiscard]]
    const char* end() const {
        return pend;
    }

private:
    char* start = nullptr;
    char* pend  = nullptr;
};
} // namespace dfdh
