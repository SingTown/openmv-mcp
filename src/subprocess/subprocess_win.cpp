#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#include <string>

#include "subprocess/subprocess.h"

namespace mcp {

Subprocess::Subprocess(const std::vector<std::string>& commands,
                       const std::filesystem::path& cwd,
                       OutputCallback callback,
                       const std::atomic<bool>& cancelled)
    : callback_(std::move(callback)), cancelled_(cancelled) {
    if (commands.empty()) {
        finished_ = true;
        exit_code_ = 0;
        return;
    }

    if (cancelled_.load()) {
        finished_ = true;
        return;
    }

    std::string compound;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            compound += " && ";
        }
        compound += commands[i];
    }

    // Job object so all child processes die together
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if (job == nullptr) {
        output_buf_.append("CreateJobObjectA() failed: " + std::to_string(GetLastError()));
        finished_ = true;
        return;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
    job_handle_ = job;

    // Pipe: write end is inheritable, read end is not
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE pipe_read = INVALID_HANDLE_VALUE;
    HANDLE pipe_write = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&pipe_read, &pipe_write, &sa, 0)) {
        output_buf_.append("CreatePipe() failed: " + std::to_string(GetLastError()));
        CloseHandle(job);
        job_handle_ = nullptr;
        finished_ = true;
        return;
    }
    SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0);

    // Redirect stdout and stderr to the pipe write end
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = pipe_write;
    si.hStdError = pipe_write;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags = STARTF_USESTDHANDLES;

    // Build "cmd.exe /c <compound>"
    char comspec[MAX_PATH] = "cmd.exe";
    GetEnvironmentVariableA("ComSpec", comspec, sizeof(comspec));
    std::string cmdline = std::string(comspec) + " /c " + compound;

    std::string cwd_str = cwd.string();
    const char* cwd_ptr = (cwd_str == ".") ? nullptr : cwd_str.c_str();

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr,
                             cmdline.data(),
                             nullptr,
                             nullptr,
                             TRUE,
                             CREATE_NO_WINDOW | CREATE_SUSPENDED,
                             nullptr,
                             cwd_ptr,
                             &si,
                             &pi);

    CloseHandle(pipe_write);

    if (!ok) {
        output_buf_.append("CreateProcessA() failed: " + std::to_string(GetLastError()));
        CloseHandle(pipe_read);
        CloseHandle(job);
        job_handle_ = nullptr;
        finished_ = true;
        return;
    }

    AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    process_handle_ = pi.hProcess;
    pipe_read_ = pipe_read;
    pid_ = static_cast<int>(pi.dwProcessId);

    int saved_pid = static_cast<int>(pi.dwProcessId);
    thread_ = std::thread([this, saved_pid]() { readLoop(saved_pid); });
}

void Subprocess::kill() {
    int old_pid = pid_.exchange(-1);
    if (old_pid > 0) {
        HANDLE job = static_cast<HANDLE>(job_handle_);
        if (job != nullptr) {
            TerminateJobObject(job, 1);
        }
        HANDLE proc = static_cast<HANDLE>(process_handle_);
        if (proc != nullptr && proc != INVALID_HANDLE_VALUE) {
            TerminateProcess(proc, 1);
        }
    }
}

void Subprocess::readLoop(int /*pid*/) {
    HANDLE h_read = static_cast<HANDLE>(pipe_read_);
    HANDLE h_proc = static_cast<HANDLE>(process_handle_);
    HANDLE h_job = static_cast<HANDLE>(job_handle_);
    std::array<char, 4096> buf{};
    bool exited_normally = false;

    auto consume = [&](DWORD n) {
        output_buf_.append(buf.data(), static_cast<size_t>(n));
        if (callback_) {
            auto text = output_buf_.take();
            if (!text.empty()) {
                callback_(text);
            }
        }
    };

    while (true) {
        if (cancelled_.load()) {
            int old = pid_.exchange(-1);
            if (old > 0) {
                if (h_job != nullptr) {
                    TerminateJobObject(h_job, 1);
                }
                TerminateProcess(h_proc, 1);
            }
            break;
        }

        DWORD bytes_avail = 0;
        if (!PeekNamedPipe(h_read, nullptr, 0, nullptr, &bytes_avail, nullptr)) {
            // Pipe broken — process has closed its end
            break;
        }

        if (bytes_avail > 0) {
            DWORD to_read = bytes_avail < static_cast<DWORD>(buf.size()) ? bytes_avail : static_cast<DWORD>(buf.size());
            DWORD bytes_read = 0;
            if (ReadFile(h_read, buf.data(), to_read, &bytes_read, nullptr) && bytes_read > 0) {
                consume(bytes_read);
            }
        } else {
            // No data; check if process exited (50 ms timeout mirrors Unix poll)
            DWORD wait_ret = WaitForSingleObject(h_proc, 50);
            if (wait_ret == WAIT_OBJECT_0) {
                // Drain any remaining output
                while (true) {
                    if (!PeekNamedPipe(h_read, nullptr, 0, nullptr, &bytes_avail, nullptr)) {
                        break;
                    }
                    if (bytes_avail == 0) {
                        break;
                    }
                    DWORD to_read =
                        bytes_avail < static_cast<DWORD>(buf.size()) ? bytes_avail : static_cast<DWORD>(buf.size());
                    DWORD bytes_read = 0;
                    if (ReadFile(h_read, buf.data(), to_read, &bytes_read, nullptr) && bytes_read > 0) {
                        consume(bytes_read);
                    } else {
                        break;
                    }
                }
                exited_normally = true;
                break;
            }
            // WAIT_TIMEOUT: continue loop
        }
    }

    if (exited_normally) {
        DWORD exit_code = 0;
        if (GetExitCodeProcess(h_proc, &exit_code)) {
            exit_code_ = static_cast<int>(exit_code);
        }
    }

    pid_.store(-1, std::memory_order_release);

    CloseHandle(h_read);
    pipe_read_ = nullptr;
    CloseHandle(h_proc);
    process_handle_ = nullptr;
    if (h_job != nullptr) {
        CloseHandle(h_job);
        job_handle_ = nullptr;
    }

    finished_ = true;
}

}  // namespace mcp

#endif  // _WIN32
