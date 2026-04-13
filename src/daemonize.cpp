#include "daemonize.h"

#include <stdexcept>
#include <string>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mcp {

void daemonize(int argc, char* argv[], const std::string& log_path) {
    // Rebuild command line, stripping --daemon / -d.
    std::string cmdline;
    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--daemon" || arg == "-d") {
            continue;
        }
        if (!cmdline.empty()) {
            cmdline += ' ';
        }
        bool needs_quote = arg.find(' ') != std::string::npos || arg.find('"') != std::string::npos;
        if (needs_quote) {
            cmdline += '"';
            for (char c : arg) {
                if (c == '"') cmdline += '\\';
                cmdline += c;
            }
            cmdline += '"';
        } else {
            cmdline += arg;
        }
    }

    // Open stdout/stderr target (NUL or log file).
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE h_nul = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    if (h_nul == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Cannot open NUL: " + std::to_string(GetLastError()));
    }

    HANDLE h_out = h_nul;
    if (!log_path.empty()) {
        h_out = CreateFileA(
            log_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h_out == INVALID_HANDLE_VALUE) {
            CloseHandle(h_nul);
            throw std::runtime_error("Cannot open log file: " + std::to_string(GetLastError()));
        }
        SetFilePointer(h_out, 0, nullptr, FILE_END);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = h_nul;
    si.hStdOutput = h_out;
    si.hStdError = h_out;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr,
                             cmdline.data(),
                             nullptr,
                             nullptr,
                             TRUE,  // inherit handles
                             DETACHED_PROCESS | CREATE_NO_WINDOW,
                             nullptr,
                             nullptr,
                             &si,
                             &pi);

    CloseHandle(h_nul);
    if (h_out != h_nul) {
        CloseHandle(h_out);
    }

    if (!ok) {
        throw std::runtime_error("CreateProcessA failed: " + std::to_string(GetLastError()));
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    ExitProcess(0);
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

void daemonize(int /*argc*/, char* /*argv*/[], const std::string& log_path) {
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
