#include "detached_process.h"

#include <httplib/httplib.h>
#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "client/mcp_client.h"
#include "openmv_version.h"

namespace mcp {

namespace {

void launchDetachedProcess(const std::vector<std::string>& args);

std::optional<std::array<int, 3>> parseVersion(std::string version) {
    if (!version.empty() && version.front() == 'v') {
        version.erase(0, 1);
    }

    std::array<int, 3> parts{};
    char dot1 = '\0';
    char dot2 = '\0';
    std::istringstream stream(version);
    stream >> std::noskipws >> parts[0] >> dot1 >> parts[1] >> dot2 >> parts[2];
    if (!stream || stream.peek() != std::char_traits<char>::eof() || dot1 != '.' || dot2 != '.' || parts[0] < 0 ||
        parts[1] < 0 || parts[2] < 0) {
        return std::nullopt;
    }

    return parts;
}

bool isVersionCompatibleWithCurrent(const std::string& version) {
    const auto running = parseVersion(version);
    const auto current = parseVersion(OPENMV_MCP_VERSION);
    return running && current && *running >= *current;
}

std::optional<std::string> openmvMcpVersionOnPort(int port) {
    try {
        McpClient client("127.0.0.1", port);
        auto info = client.initialize();
        auto server_info = info.value("serverInfo", json::object());
        if (!server_info.is_object() || server_info.value("name", "") != "openmv-mcp-server") {
            return std::nullopt;
        }

        const auto version = server_info.find("version");
        if (version != server_info.end() && version->is_string()) {
            return version->get<std::string>();
        }
        return std::string{};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace

void shutdownServer(int port);

void ensureServerRunning(const std::string& executable,
                         int port,
                         const std::string& log_path,
                         const std::string& log_level) {
    const auto version = openmvMcpVersionOnPort(port);
    if (version) {
        if (isVersionCompatibleWithCurrent(*version)) {
            return;
        }

        const auto running_version = version->empty() ? std::string("<unknown>") : *version;
        spdlog::info("Restarting openmv-mcp server on port {} (running version {}, current version {})",
                     port,
                     running_version,
                     OPENMV_MCP_VERSION);
        shutdownServer(port);
        for (int i = 0; openmvMcpVersionOnPort(port); ++i) {
            if (i == 40) {
                throw std::runtime_error("older openmv-mcp HTTP server did not stop in time");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    std::vector<std::string> args{executable, "--mode", "internal_server", "--port", std::to_string(port)};
    if (!log_path.empty()) {
        args.emplace_back("--log");
        args.push_back(log_path);
    }
    if (!log_level.empty()) {
        args.emplace_back("--level");
        args.push_back(log_level);
    }
    launchDetachedProcess(args);

    for (int i = 0; i < 40; ++i) {
        const auto ready_version = openmvMcpVersionOnPort(port);
        if (ready_version && isVersionCompatibleWithCurrent(*ready_version)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    throw std::runtime_error("openmv-mcp HTTP server did not become ready in time");
}

void shutdownServer(int port) {
    if (!openmvMcpVersionOnPort(port)) {
        return;
    }
    httplib::Client cli("127.0.0.1", port);
    auto resp = cli.Post("/shutdown", "", "application/json");
    if (!resp || resp->status != 200) {
        throw std::runtime_error("failed to stop openmv-mcp HTTP server");
    }
}

}  // namespace mcp

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mcp {

namespace {

std::runtime_error windowsError(const std::string& action, DWORD error = GetLastError()) {
    return std::runtime_error(action + " failed: " + std::to_string(error));
}

std::wstring toWideString(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    UINT code_page = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (size == 0) {
        code_page = CP_ACP;
        flags = 0;
        size = MultiByteToWideChar(code_page, flags, value.c_str(), -1, nullptr, 0);
    }
    if (size == 0) {
        throw windowsError("MultiByteToWideChar");
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    if (MultiByteToWideChar(code_page, flags, value.c_str(), -1, wide.data(), size) == 0) {
        throw windowsError("MultiByteToWideChar");
    }
    wide.resize(static_cast<size_t>(size - 1));
    return wide;
}

std::wstring quoteWindowsArg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needs_quote = false;
    for (wchar_t c : arg) {
        if (c == L' ' || c == L'\t' || c == L'"') {
            needs_quote = true;
            break;
        }
    }
    if (!needs_quote) {
        return arg;
    }

    std::wstring quoted = L"\"";
    int backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            quoted.append(static_cast<size_t>(backslashes * 2 + 1), L'\\');
            quoted.push_back(c);
        } else {
            quoted.append(static_cast<size_t>(backslashes), L'\\');
            quoted.push_back(c);
        }
        backslashes = 0;
    }
    quoted.append(static_cast<size_t>(backslashes * 2), L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring buildWindowsCommandLine(const std::vector<std::string>& args) {
    std::wstring cmdline;
    for (const auto& arg : args) {
        if (!cmdline.empty()) {
            cmdline.push_back(L' ');
        }
        cmdline += quoteWindowsArg(toWideString(arg));
    }
    return cmdline;
}

void launchDetachedProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        throw std::runtime_error("detached process launcher requires an executable");
    }

    auto cmdline = buildWindowsCommandLine(args);

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE h_nul = CreateFileW(
        L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    if (h_nul == INVALID_HANDLE_VALUE) {
        throw windowsError("CreateFileW(NUL)");
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = h_nul;
    si.StartupInfo.hStdOutput = h_nul;
    si.StartupInfo.hStdError = h_nul;

    SIZE_T attribute_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attribute_size);
    if (attribute_size == 0) {
        DWORD error = GetLastError();
        CloseHandle(h_nul);
        throw windowsError("InitializeProcThreadAttributeList", error);
    }

    std::vector<unsigned char> attribute_buffer(attribute_size);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_buffer.data());
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attribute_size)) {
        DWORD error = GetLastError();
        CloseHandle(h_nul);
        throw windowsError("InitializeProcThreadAttributeList", error);
    }

    HANDLE inherited_handles[] = {h_nul};
    if (!UpdateProcThreadAttribute(si.lpAttributeList,
                                   0,
                                   PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherited_handles,
                                   sizeof(inherited_handles),
                                   nullptr,
                                   nullptr)) {
        DWORD error = GetLastError();
        DeleteProcThreadAttributeList(si.lpAttributeList);
        CloseHandle(h_nul);
        throw windowsError("UpdateProcThreadAttribute", error);
    }

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr,
                             cmdline.data(),
                             nullptr,
                             nullptr,
                             TRUE,
                             EXTENDED_STARTUPINFO_PRESENT | DETACHED_PROCESS | CREATE_NO_WINDOW,
                             nullptr,
                             nullptr,
                             &si.StartupInfo,
                             &pi);
    DWORD create_error = ok ? ERROR_SUCCESS : GetLastError();

    DeleteProcThreadAttributeList(si.lpAttributeList);
    CloseHandle(h_nul);

    if (!ok) {
        throw windowsError("CreateProcessW", create_error);
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

}  // namespace

}  // namespace mcp

#else

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace mcp {

namespace {

enum class LaunchStep : int {
    Setsid = 1,
    SecondFork,
    OpenStdin,
    DupStdin,
    OpenStdout,
    DupStdout,
    OpenStderr,
    DupStderr,
    Exec,
};

struct LaunchError {
    int step;
    int error;
};

const char* launchStepName(int step) {
    switch (static_cast<LaunchStep>(step)) {
        case LaunchStep::Setsid:
            return "setsid";
        case LaunchStep::SecondFork:
            return "second fork";
        case LaunchStep::OpenStdin:
            return "open stdin";
        case LaunchStep::DupStdin:
            return "dup stdin";
        case LaunchStep::OpenStdout:
            return "open stdout";
        case LaunchStep::DupStdout:
            return "dup stdout";
        case LaunchStep::OpenStderr:
            return "open stderr";
        case LaunchStep::DupStderr:
            return "dup stderr";
        case LaunchStep::Exec:
            return "execvp";
    }
    return "daemon launch";
}

void setCloseOnExecOrThrow(int fd) {
    int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
        throw std::runtime_error(std::string("fcntl(FD_CLOEXEC) failed: ") + std::strerror(errno));
    }
}

void closeFd(int fd) {
    while (::close(fd) < 0 && errno == EINTR) {
    }
}

void writeLaunchErrorAndExit(int fd, LaunchStep step, int error) {
    LaunchError launch_error{static_cast<int>(step), error};
    const char* data = reinterpret_cast<const char*>(&launch_error);
    size_t remaining = sizeof(launch_error);
    while (remaining > 0) {
        ssize_t written = ::write(fd, data, remaining);
        if (written > 0) {
            data += written;
            remaining -= static_cast<size_t>(written);
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    _exit(127);
}

void redirectFdOrReport(
    int target_fd, const char* path, int flags, int error_fd, LaunchStep open_step, LaunchStep dup_step) {
    int fd = ::open(path, flags, 0644);
    if (fd < 0) {
        writeLaunchErrorAndExit(error_fd, open_step, errno);
    }
    if (::dup2(fd, target_fd) < 0) {
        int saved_errno = errno;
        closeFd(fd);
        writeLaunchErrorAndExit(error_fd, dup_step, saved_errno);
    }
    if (fd != target_fd) {
        closeFd(fd);
    }
}

bool readLaunchError(int fd, LaunchError* launch_error) {
    char* data = reinterpret_cast<char*>(launch_error);
    size_t remaining = sizeof(*launch_error);
    bool received = false;
    while (remaining > 0) {
        ssize_t bytes_read = ::read(fd, data, remaining);
        if (bytes_read > 0) {
            received = true;
            data += bytes_read;
            remaining -= static_cast<size_t>(bytes_read);
        } else if (bytes_read == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            throw std::runtime_error(std::string("read detached process launcher pipe failed: ") +
                                     std::strerror(errno));
        }
    }
    if (received && remaining != 0) {
        throw std::runtime_error("detached process launcher returned a partial error report");
    }
    return received;
}

void launchDetachedProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        throw std::runtime_error("detached process launcher requires an executable");
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    int error_pipe[2];
    if (::pipe(error_pipe) < 0) {
        throw std::runtime_error(std::string("pipe failed: ") + std::strerror(errno));
    }

    try {
        setCloseOnExecOrThrow(error_pipe[1]);
    } catch (...) {
        closeFd(error_pipe[0]);
        closeFd(error_pipe[1]);
        throw;
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        int saved_errno = errno;
        closeFd(error_pipe[0]);
        closeFd(error_pipe[1]);
        errno = saved_errno;
        throw std::runtime_error(std::string("fork failed: ") + std::strerror(errno));
    }

    if (pid == 0) {
        closeFd(error_pipe[0]);

        if (::setsid() < 0) {
            writeLaunchErrorAndExit(error_pipe[1], LaunchStep::Setsid, errno);
        }

        pid_t grandchild = ::fork();
        if (grandchild < 0) {
            writeLaunchErrorAndExit(error_pipe[1], LaunchStep::SecondFork, errno);
        }
        if (grandchild > 0) {
            _exit(0);
        }

        redirectFdOrReport(
            STDIN_FILENO, "/dev/null", O_RDONLY, error_pipe[1], LaunchStep::OpenStdin, LaunchStep::DupStdin);
        redirectFdOrReport(
            STDOUT_FILENO, "/dev/null", O_WRONLY, error_pipe[1], LaunchStep::OpenStdout, LaunchStep::DupStdout);
        redirectFdOrReport(
            STDERR_FILENO, "/dev/null", O_WRONLY, error_pipe[1], LaunchStep::OpenStderr, LaunchStep::DupStderr);

        ::execvp(argv[0], argv.data());
        writeLaunchErrorAndExit(error_pipe[1], LaunchStep::Exec, errno);
    }

    closeFd(error_pipe[1]);

    LaunchError launch_error{};
    bool has_launch_error = false;
    try {
        has_launch_error = readLaunchError(error_pipe[0], &launch_error);
    } catch (...) {
        closeFd(error_pipe[0]);
        throw;
    }
    closeFd(error_pipe[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            throw std::runtime_error(std::string("waitpid failed: ") + std::strerror(errno));
        }
    }
    if (has_launch_error) {
        throw std::runtime_error(std::string(launchStepName(launch_error.step)) +
                                 " failed: " + std::strerror(launch_error.error));
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        throw std::runtime_error("detached process launcher exited with code " +
                                 std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1));
    }
}

}  // namespace

}  // namespace mcp

#endif
