#include "daemonize.h"

#include <stdexcept>
#include <string>

#ifdef _WIN32

namespace mcp {
void daemonize(const std::string&) {
    throw std::runtime_error("--daemon is not supported on Windows");
}
}  // namespace mcp

#else

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace mcp {

namespace {

void redirectFd(int target_fd, const char* path, int flags) {
    int fd = ::open(path, flags, 0644);
    if (fd < 0) {
        throw std::runtime_error(std::string("open ") + path + " failed: " + std::strerror(errno));
    }
    if (::dup2(fd, target_fd) < 0) {
        int saved = errno;
        ::close(fd);
        throw std::runtime_error(std::string("dup2 failed: ") + std::strerror(saved));
    }
    if (fd != target_fd) {
        ::close(fd);
    }
}

}  // namespace

void daemonize(const std::string& log_path) {
    pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error(std::string("first fork failed: ") + std::strerror(errno));
    }
    if (pid > 0) {
        _exit(0);
    }

    if (::setsid() < 0) {
        throw std::runtime_error(std::string("setsid failed: ") + std::strerror(errno));
    }

    pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error(std::string("second fork failed: ") + std::strerror(errno));
    }
    if (pid > 0) {
        _exit(0);
    }

    if (::chdir("/") < 0) {
        throw std::runtime_error(std::string("chdir(/) failed: ") + std::strerror(errno));
    }
    ::umask(0);

    redirectFd(STDIN_FILENO, "/dev/null", O_RDONLY);

    const char* out_path = log_path.empty() ? "/dev/null" : log_path.c_str();
    int out_flags = log_path.empty() ? O_WRONLY : (O_WRONLY | O_CREAT | O_APPEND);
    redirectFd(STDOUT_FILENO, out_path, out_flags);
    redirectFd(STDERR_FILENO, out_path, out_flags);
}

}  // namespace mcp

#endif
