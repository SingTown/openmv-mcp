#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#include <windows.h>

#include "serial_port.h"

namespace mcp {

static constexpr DWORD kCommBufSize = 64 * 1024 * 1024;

static bool set_baud_rate(HANDLE h, DWORD baud) {
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) return false;
    dcb.BaudRate = baud;
    return SetCommState(h, &dcb) != 0;
}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& path) {
    close();

    // Normalize COM port path: prepend \\.\  if not already present
    std::string dev_path = path;
    if (path.size() < 4 || path.substr(0, 4) != "\\\\.\\") {
        dev_path = "\\\\.\\" + path;
    }

    HANDLE h = CreateFileA(
        dev_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    handle_ = h;

    // Create manual-reset events for overlapped I/O
    read_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    write_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!read_event_ || !write_event_) {
        close();
        return false;
    }

    // Set large driver-level buffers for frame data transfer (matches OpenMV IDE: 64MB)
    SetupComm(h, kCommBufSize, kCommBufSize);

    // Configure DCB: 8N1, no flow control, DTR enabled
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        close();
        return false;
    }

    dcb.fBinary = TRUE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(h, &dcb)) {
        close();
        return false;
    }

    // Set baud rate: try 921600, fallback to 12000000
    if (!set_baud_rate(h, 921600) && !set_baud_rate(h, 12000000)) {
        close();
        return false;
    }

    // Timeouts: return immediately if data in buffer, or wait up to 10ms
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 10;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 3000;
    if (!SetCommTimeouts(h, &timeouts)) {
        close();
        return false;
    }

    // Purge buffers
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return true;
}

void SerialPort::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        auto h = static_cast<HANDLE>(handle_);
        CancelIo(h);
        PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
        CloseHandle(h);
        handle_ = INVALID_HANDLE_VALUE;
    }
    if (read_event_) {
        CloseHandle(static_cast<HANDLE>(read_event_));
        read_event_ = nullptr;
    }
    if (write_event_) {
        CloseHandle(static_cast<HANDLE>(write_event_));
        write_event_ = nullptr;
    }
    write_buf_.clear();
    recv_buf_.clear();
}

void SerialPort::purge() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        auto h = static_cast<HANDLE>(handle_);
        CancelIo(h);
        PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }
    recv_buf_.clear();
}

bool SerialPort::isOpen() const {
    return handle_ != INVALID_HANDLE_VALUE;
}

bool SerialPort::send() {
    if (handle_ == INVALID_HANDLE_VALUE) {
        write_buf_.clear();
        return false;
    }
    auto h = static_cast<HANDLE>(handle_);
    size_t total = write_buf_.size();
    spdlog::debug("[serial] TX {}: {:spn}",
                  total,
                  spdlog::to_hex(write_buf_.data(), write_buf_.data() + std::min(total, size_t{64})));
    size_t written = 0;
    while (written < total) {
        OVERLAPPED ov{};
        ov.hEvent = static_cast<HANDLE>(write_event_);
        ResetEvent(ov.hEvent);

        auto remaining = static_cast<DWORD>(total - written);
        DWORD n = 0;
        BOOL ok = WriteFile(h, write_buf_.data() + written, remaining, nullptr, &ov);
        if (!ok && GetLastError() != ERROR_IO_PENDING) {
            write_buf_.clear();
            return false;
        }
        // Always use GetOverlappedResult for overlapped I/O
        if (!GetOverlappedResult(h, &ov, &n, TRUE)) {
            write_buf_.clear();
            return false;
        }
        if (n == 0) {
            write_buf_.clear();
            return false;
        }
        written += n;
    }
    write_buf_.clear();
    return true;
}

bool SerialPort::recv() {
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    auto h = static_cast<HANDLE>(handle_);

    OVERLAPPED ov{};
    ov.hEvent = static_cast<HANDLE>(read_event_);
    ResetEvent(ov.hEvent);

    uint8_t buf[4096];
    DWORD bytes_read = 0;
    BOOL ok = ReadFile(h, buf, sizeof(buf), nullptr, &ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) return false;

    // Wait for completion (bounded by COMMTIMEOUTS: ~10ms)
    if (!GetOverlappedResult(h, &ov, &bytes_read, TRUE)) return false;

    if (bytes_read > 0) {
        spdlog::debug("[serial] RX {}: {:spn}", bytes_read, spdlog::to_hex(buf, buf + std::min<DWORD>(bytes_read, 64)));
        recv_buf_.push_back(buf, bytes_read);
        return true;
    }
    return false;
}

}  // namespace mcp

#endif  // _WIN32
