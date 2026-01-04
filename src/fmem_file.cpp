#include <cstring>
#include <stdexcept>

#include <otfccxx-lib_private/fmem_file.hpp>

namespace otfccxx {
fmem_file::fmem_file(std::span<const std::byte> data) : mem_(new fmem{}), file_(nullptr) {
    fmem_init(mem_);

    file_ = fmem_open(mem_, "wb+");
    if (! file_) {
        fmem_term(mem_);
        delete mem_;
        throw std::runtime_error("fmem_open failed");
    }

    if (! data.empty()) {
        const size_t written = fwrite(data.data(), 1, data.size(), file_);

        if (written != data.size()) {
            fclose(file_);
            fmem_term(mem_);
            delete mem_;
            throw std::runtime_error("fwrite failed");
        }
    }

    fflush(file_);
    rewind(file_);
}

fmem_file::~fmem_file() {
    if (file_) { fclose(file_); }

    if (mem_) {
        fmem_term(mem_);
        delete mem_;
    }
}

fmem_file::fmem_file(fmem_file &&other) noexcept : mem_(other.mem_), file_(other.file_) {
    other.mem_  = nullptr;
    other.file_ = nullptr;
}

fmem_file &
fmem_file::operator=(fmem_file &&other) noexcept {
    if (this != &other) {
        if (file_) { fclose(file_); }

        if (mem_) {
            fmem_term(mem_);
            delete mem_;
        }

        mem_  = other.mem_;
        file_ = other.file_;

        other.mem_  = nullptr;
        other.file_ = nullptr;
    }
    return *this;
}

FILE *
fmem_file::get() const noexcept {
    return file_;
}
} // namespace otfccxx