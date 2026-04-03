#include "serial_port.h"

#ifndef _WIN32

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <IOKit/serial/ioss.h>
#endif

namespace mcp {

SerialPort::~SerialPort() {
    close();
}

// Set baud rate using IOSSIOSPEED (macOS) or termios/termios2 (Linux).
static bool set_baud_rate(int fd, int baud) {
#ifdef __APPLE__
    auto speed = static_cast<speed_t>(baud);
    return ioctl(fd, IOSSIOSPEED, &speed) == 0;
#else
    // Try standard constant first (921600)
#ifdef B921600
    if (baud == 921600) {
        struct termios tty{};
        if (tcgetattr(fd, &tty) != 0) {
            return false;
        }
        cfsetispeed(&tty, B921600);
        cfsetospeed(&tty, B921600);
        return tcsetattr(fd, TCSANOW, &tty) == 0;
    }
#endif
    // Custom baud rate via BOTHER + termios2
#if defined(BOTHER) && defined(TCSETS2)
    struct termios2 tty2{};
    if (ioctl(fd, TCGETS2, &tty2) < 0) {
        return false;
    }
    tty2.c_cflag &= ~CBAUD;
    tty2.c_cflag |= BOTHER;
    tty2.c_ispeed = baud;
    tty2.c_ospeed = baud;
    return ioctl(fd, TCSETS2, &tty2) == 0;
#else
    return false;
#endif
#endif
}

bool SerialPort::open(const std::string& path) {
    close();

    fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        return false;
    }

    ioctl(fd_, TIOCEXCL);

    struct termios tty{};
    if (tcgetattr(fd_, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    cfmakeraw(&tty);
    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= CS8 | CLOCAL | CREAD;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (!set_baud_rate(fd_, 921600) && !set_baud_rate(fd_, 12000000)) {
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    tcflush(fd_, TCIOFLUSH);

    int flag = TIOCM_DTR;
    ioctl(fd_, TIOCMBIS, &flag);

    return true;
}

void SerialPort::close() {
    if (fd_ >= 0) {
        tcdrain(fd_);
        ::close(fd_);
        fd_ = -1;
    }
    write_buf_.clear();
    recv_buf_.clear();
}

bool SerialPort::isOpen() const {
    return fd_ >= 0;
}

bool SerialPort::send() {
    if (fd_ < 0) {
        write_buf_.clear();
        return false;
    }
    size_t len = write_buf_.size();
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd_, write_buf_.data() + written, len - written);
        if (n < 0) {
            write_buf_.clear();
            return false;
        }
        written += static_cast<size_t>(n);
    }
    tcdrain(fd_);
    write_buf_.clear();
    return true;
}

bool SerialPort::recv() {
    if (fd_ < 0) return false;

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd_, &rfds);
    struct timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (select(fd_ + 1, &rfds, nullptr, nullptr, &tv) <= 0) return false;

    uint8_t buf[4096];
    ssize_t n = ::read(fd_, buf, sizeof(buf));
    if (n > 0) {
        recv_buf_.push_back(buf, static_cast<size_t>(n));
        return true;
    }
    return false;
}

}  // namespace mcp

#endif  // !_WIN32
