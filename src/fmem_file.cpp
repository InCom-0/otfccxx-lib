#include <cstring>
#include <stdexcept>

#include <otfccxx-lib_private/fmem_file.hpp>

namespace otfccxx {
fmem_file::fmem_file() : mem_(new fmem{}) {
    fmem_init(mem_);
}

fmem_file::~fmem_file() {
    if (mem_) {
        fmem_term(mem_);
        delete mem_;
    }
}

fmem_file::fmem_file(fmem_file &&other) noexcept : mem_(other.mem_) {
    other.mem_ = nullptr;
}

fmem_file &
fmem_file::operator=(fmem_file &&other) noexcept {
    if (this != &other) {

        if (mem_) {
            fmem_term(mem_);
            delete mem_;
        }

        mem_ = other.mem_;

        other.mem_ = nullptr;
    }
    return *this;
}

// Attach and access
// You own the file afterwards but not the underlying data
FILE *
fmem_file::attach(std::span<const std::byte> data) const {

    FILE *resFile = nullptr;

    resFile = fmem_open(mem_, "wb+");
    if (! resFile) {
        fmem_term(mem_);
        delete mem_;
        throw std::runtime_error("fmem_open failed");
    }

    if (! data.empty()) {
        const size_t written = fwrite(data.data(), 1, data.size(), resFile);

        if (written != data.size()) {
            fclose(resFile);
            fmem_term(mem_);
            delete mem_;
            throw std::runtime_error("fwrite failed");
        }
    }

    fflush(resFile);
    rewind(resFile);
    return resFile;
}
} // namespace otfccxx