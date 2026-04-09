#include "subprocess/subprocess.h"

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace mcp {

TEST(SubprocessTest, EchoOutput) {
    Subprocess proc({"echo hello world"});
    proc.join();
    EXPECT_TRUE(proc.finished());
    EXPECT_EQ(proc.exitCode(), 0);
#ifdef _WIN32
    EXPECT_EQ(proc.readOutput(), "hello world\r\n");
#else
    EXPECT_EQ(proc.readOutput(), "hello world\n");
#endif
}

TEST(SubprocessTest, ExitCode) {
    Subprocess proc({"exit 42"});
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
    EXPECT_EQ(proc.exitCode(), 42);
}

TEST(SubprocessTest, StderrCaptured) {
    Subprocess proc({"echo err 1>&2"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
#ifdef _WIN32
    EXPECT_EQ(proc.readOutput(), "err \r\n");
#else
    EXPECT_EQ(proc.readOutput(), "err\n");
#endif
}

TEST(SubprocessTest, InvalidExecutable) {
    Subprocess proc({"__nonexistent_binary_12345__"});
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
    EXPECT_NE(proc.exitCode(), 0);
}

TEST(SubprocessTest, WorkingDirectory) {
#ifdef _WIN32
    Subprocess proc({"cd"}, "C:\\Windows\\Temp");
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_EQ(proc.readOutput(), "C:\\Windows\\Temp\r\n");
#else
    Subprocess proc({"pwd"}, "/tmp");
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    std::string output = proc.readOutput();
    // macOS: /tmp -> /private/tmp
    EXPECT_TRUE(output == "/tmp\n" || output == "/private/tmp\n");
#endif
}

TEST(SubprocessTest, Kill) {
#ifdef _WIN32
    Subprocess proc({"ping -n 60 127.0.0.1 > nul"});
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#else
    Subprocess proc({"sleep 60"});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
    EXPECT_FALSE(proc.finished());
    proc.kill();
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
    EXPECT_NE(proc.exitCode(), 0);
}

TEST(SubprocessTest, MultilineOutput) {
#ifdef _WIN32
    Subprocess proc({"echo line1 & echo line2 & echo line3"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_EQ(proc.readOutput(), "line1 \r\nline2 \r\nline3\r\n");
#else
    Subprocess proc({"echo line1; echo line2; echo line3"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_EQ(proc.readOutput(), "line1\nline2\nline3\n");
#endif
}

TEST(SubprocessTest, ReadOutputBeforeFinished) {
#ifdef _WIN32
    Subprocess proc({"echo early & ping -n 2 127.0.0.1 > nul & echo late"});
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
#else
    Subprocess proc({"echo early; sleep 0.2; echo late"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
    std::string partial = proc.readOutput();
    EXPECT_FALSE(partial.empty());
    proc.join();
    std::string all = partial + proc.readOutput();
#ifdef _WIN32
    EXPECT_EQ(all, "early \r\nlate\r\n");
#else
    EXPECT_EQ(all, "early\nlate\n");
#endif
}

TEST(SubprocessTest, LargeOutput) {
#ifdef _WIN32
    Subprocess proc({"powershell -NoProfile -Command \"'A' * 102400\""});
#else
    Subprocess proc({"dd if=/dev/zero bs=1024 count=100 2>/dev/null | base64"});
#endif
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_GT(proc.readOutput().size(), 100000U);
}

TEST(SubprocessTest, StreamingReadWithoutJoin) {
#ifdef _WIN32
    Subprocess proc({"for /L %i in (1,1,5) do @(echo line%i & ping -n 2 127.0.0.1 > nul)"});
#else
    Subprocess proc({"for i in 1 2 3 4 5; do echo line$i; sleep 0.1; done"});
#endif
    std::string all;
    while (!proc.finished()) {
        all += proc.readOutput();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    all += proc.readOutput();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_TRUE(all.find("line1") != std::string::npos);
    EXPECT_TRUE(all.find("line5") != std::string::npos);
    int count = 0;
    for (size_t pos = 0; (pos = all.find("line", pos)) != std::string::npos; ++pos) {
        ++count;
    }
    EXPECT_EQ(count, 5);
}

TEST(SubprocessTest, DestructorJoins) {
    {
        Subprocess proc({"echo test"});
    }
    // No crash or hang = pass
}

TEST(SubprocessTest, EmptyCommands) {
    Subprocess proc({});
    EXPECT_TRUE(proc.finished());
    EXPECT_EQ(proc.exitCode(), 0);
}

TEST(SubprocessTest, MultipleCommandsSequential) {
    Subprocess proc({"echo first", "echo second", "echo third"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
#ifdef _WIN32
    EXPECT_EQ(proc.readOutput(), "first \r\nsecond \r\nthird\r\n");
#else
    EXPECT_EQ(proc.readOutput(), "first\nsecond\nthird\n");
#endif
}

TEST(SubprocessTest, MultipleCommandsStopOnFailure) {
    Subprocess proc({"echo before", "exit 1", "echo after"});
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_NE(proc.exitCode(), 0);
#ifdef _WIN32
    EXPECT_EQ(proc.readOutput(), "before \r\n");
#else
    EXPECT_EQ(proc.readOutput(), "before\n");
#endif
}

TEST(SubprocessTest, Callback) {
    std::mutex mtx;
    std::string collected;
    auto callback = [&](const std::string& text) {
        std::lock_guard<std::mutex> lock(mtx);
        collected += text;
    };
    Subprocess proc({"echo hello", "echo world"}, ".", callback);
    proc.join();
    std::lock_guard<std::mutex> lock(mtx);
#ifdef _WIN32
    EXPECT_EQ(collected, "hello \r\nworld\r\n");
#else
    EXPECT_EQ(collected, "hello\nworld\n");
#endif
}

TEST(SubprocessTest, CallbackStreaming) {
    std::mutex mtx;
    int call_count = 0;
    auto callback = [&](const std::string& /*text*/) {
        std::lock_guard<std::mutex> lock(mtx);
        ++call_count;
    };
#ifdef _WIN32
    Subprocess proc({"for /L %i in (1,1,5) do @(echo line%i & ping -n 2 127.0.0.1 > nul)"}, ".", callback);
#else
    Subprocess proc({"for i in 1 2 3 4 5; do echo line$i; sleep 0.1; done"}, ".", callback);
#endif
    proc.join();
    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_GT(call_count, 0);
}

TEST(SubprocessTest, Cancelled) {
    std::atomic<bool> cancelled{false};
#ifdef _WIN32
    Subprocess proc({"ping -n 60 127.0.0.1 > nul"}, ".", nullptr, cancelled);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#else
    Subprocess proc({"sleep 60"}, ".", nullptr, cancelled);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
    EXPECT_FALSE(proc.finished());
    cancelled = true;
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
}

TEST(SubprocessTest, CancelledWithCallback) {
    std::atomic<bool> cancelled{false};
    std::mutex mtx;
    std::string collected;
    auto callback = [&](const std::string& text) {
        std::lock_guard<std::mutex> lock(mtx);
        collected += text;
    };
#ifdef _WIN32
    Subprocess proc({"for /L %i in (1,1,100) do @(echo line%i & ping -n 2 127.0.0.1 > nul)"}, ".", callback, cancelled);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
#else
    Subprocess proc({"for i in $(seq 1 100); do echo line$i; sleep 0.1; done"}, ".", callback, cancelled);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif
    cancelled = true;
    EXPECT_THROW(proc.join(), std::runtime_error);
    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_FALSE(collected.empty());
}

TEST(SubprocessTest, CancelledBeforeStart) {
    std::atomic<bool> cancelled{true};
    Subprocess proc({"echo should not run"}, ".", nullptr, cancelled);
    EXPECT_THROW(proc.join(), std::runtime_error);
}

}  // namespace mcp
