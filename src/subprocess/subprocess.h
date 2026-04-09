#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/types.h>
#endif

#include "utils/utf8_buffer.h"

namespace mcp {

using OutputCallback = std::function<void(const std::string&)>;

inline const std::atomic<bool> kNotCancelled{false};

class Subprocess {
 public:
    Subprocess(const std::vector<std::string>& commands,
               const std::filesystem::path& cwd = ".",
               OutputCallback callback = nullptr,
               const std::atomic<bool>& cancelled = kNotCancelled);
    ~Subprocess();
    Subprocess(const Subprocess&) = delete;
    Subprocess& operator=(const Subprocess&) = delete;
    Subprocess(Subprocess&&) = delete;
    Subprocess& operator=(Subprocess&&) = delete;

    void kill();
    void join();
    std::string readOutput();
    bool finished() const;
    int exitCode() const;

 private:
#ifdef _WIN32
    std::atomic<int> pid_{-1};
#else
    std::atomic<pid_t> pid_{-1};
#endif
    int pipe_fd_ = -1;
    std::thread thread_;
    Utf8Buffer output_buf_;
    OutputCallback callback_;
    const std::atomic<bool>& cancelled_;
#ifdef _WIN32
    void readLoop(int pid);
#else
    void readLoop(pid_t pid);
#endif
    bool joined_ = false;
    std::atomic<bool> finished_{false};
    std::atomic<int> exit_code_{-1};
};

}  // namespace mcp
