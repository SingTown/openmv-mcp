#ifndef _WIN32

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>

#include "subprocess/subprocess.h"

namespace mcp {

Subprocess::Subprocess(const std::vector<std::string>& commands, const std::filesystem::path& cwd) {
    if (commands.empty()) {
        finished_ = true;
        exit_code_ = 0;
        return;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        std::string err = "pipe() failed: " + std::string(strerror(errno));
        output_buf_.append(err);
        finished_ = true;
        return;
    }
    // Prevent pipe read-end from leaking into future child processes
    fcntl(pipe_fd[0], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        std::string err = "fork() failed: " + std::string(strerror(errno));
        output_buf_.append(err);
        finished_ = true;
        return;
    }

    if (pid == 0) {
        setpgid(0, 0);
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        if (chdir(cwd.c_str()) != 0) {
            _exit(127);
        }

        std::string compound;
        for (size_t i = 0; i < commands.size(); ++i) {
            if (i > 0) {
                compound += " && ";
            }
            compound += commands[i];
        }
        execl("/bin/sh", "sh", "-c", compound.c_str(), nullptr);  // NOLINT
        _exit(127);
    }

    close(pipe_fd[1]);
    setpgid(pid, pid);  // Mirror child's setpgid to close the race window
    pid_ = pid;
    pipe_fd_ = pipe_fd[0];

    pid_t child_pid = pid;
    thread_ = std::thread([this, child_pid]() { readLoop(child_pid); });
}

void Subprocess::kill() {
    pid_t pid = pid_.exchange(-1);
    if (pid > 0) {
        ::kill(-pid, SIGKILL);
    }
}

void Subprocess::readLoop(pid_t pid) {
    int status = 0;
    bool exited_normally = false;
    std::array<char, 4096> buf{};

    auto consume = [&](ssize_t n) { output_buf_.append(buf.data(), static_cast<size_t>(n)); };

    struct pollfd pfd{};
    pfd.fd = pipe_fd_;
    pfd.events = POLLIN;

    while (true) {
        int poll_ret = poll(&pfd, 1, 50);

        if (poll_ret == -1 && errno != EINTR) {
            break;
        }

        if (poll_ret > 0 && ((pfd.revents & (POLLIN | POLLHUP)) != 0)) {
            ssize_t n = read(pipe_fd_, buf.data(), buf.size());
            if (n > 0) {
                consume(n);
            }
        }

        int ret = waitpid(pid, &status, WNOHANG);
        if (ret == pid) {
            exited_normally = true;
            fcntl(pipe_fd_, F_SETFL, O_NONBLOCK);
            while (true) {
                ssize_t n = read(pipe_fd_, buf.data(), buf.size());
                if (n > 0) {
                    consume(n);
                } else if (n == -1 && errno == EINTR) {
                    continue;
                } else {
                    break;
                }
            }
            break;
        }
        if (ret == -1) {
            break;
        }
    }

    close(pipe_fd_);
    pid_.store(-1, std::memory_order_release);

    if (exited_normally && WIFEXITED(status)) {
        exit_code_ = WEXITSTATUS(status);
    } else if (exited_normally && WIFSIGNALED(status)) {
        exit_code_ = 128 + WTERMSIG(status);
    }
    finished_ = true;
}

}  // namespace mcp

#endif  // !_WIN32
