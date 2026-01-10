#pragma once

#include <cstddef>
#include <cstdio>
#include <span>

// Forward-declare fmem (opaque to users of the header)
extern "C" {
#include <fmem.h>
}

namespace otfccxx {

struct fmem_file {
public:
    // Construct from memory (copies data into memory-backed FILE*)
    explicit fmem_file();

    // Destructor
    ~fmem_file();

    // non-copyable
    fmem_file(const fmem_file &) = delete;
    fmem_file &
    operator=(const fmem_file &) = delete;

    // movable
    fmem_file(fmem_file &&other) noexcept;
    fmem_file &
    operator=(fmem_file &&other) noexcept;

    // attach and access
    FILE *
    attach(std::span<const std::byte> data) const;


private:
    fmem *mem_; // owned
};

} // namespace otfccxx