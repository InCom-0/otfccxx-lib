
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>


#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define dup   _dup
#define dup2  _dup2
#define close _close
#define read  _read
#else
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define fileno _fileno
#endif


#include <otfccxx-lib_private/machinery_stderr_capt.hpp>


namespace otfccxx {
namespace detail {

_stderrCapture::_stderrCapture() {
#ifdef _WIN32
    if (_pipe(pipe_fd_, 4096, _O_BINARY) == -1) { throw std::runtime_error("_pipe() failed"); }
#else
    if (pipe(pipe_fd_) == -1) { throw std::runtime_error("pipe() failed"); }
#endif

    saved_stderr_ = dup(fileno(stderr));
    if (saved_stderr_ == -1) { throw std::runtime_error("dup(stderr) failed"); }

    if (dup2(pipe_fd_[1], fileno(stderr)) == -1) { throw std::runtime_error("dup2(stderr) failed"); }

    close(pipe_fd_[1]);
    pipe_fd_[1] = -1;
}
_stderrCapture::~_stderrCapture() {
    restore();
}

void
_stderrCapture::restore() {
    if (restored_) { return; }

    fflush(stderr);

    if (saved_stderr_ != -1) {
        dup2(saved_stderr_, fileno(stderr));
        close(saved_stderr_);
        saved_stderr_ = -1;
    }

    restored_ = true;
}


std::vector<std::string>
_stderrCapture::read_lines() {
    restore();

    // std::string buffer;
    char tmp[4096];
    int  n;

    std::vector<std::string> lines;
    while ((n = read(pipe_fd_[0], tmp, sizeof(tmp))) > 0) { lines.push_back(std::string(tmp, n)); }

    close(pipe_fd_[0]);
    pipe_fd_[0] = -1;

    // std::istringstream       iss(buffer);
    // std::string              line;

    // while (std::getline(iss, line)) { lines.push_back(line); }

    return lines;
}


} // namespace detail
} // namespace otfccxx