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
    EXPECT_EQ(proc.readOutput(), "hello world\n");
}

TEST(SubprocessTest, ExitCode) {
    Subprocess proc({"exit 42"});
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
    EXPECT_EQ(proc.exitCode(), 42);
}

TEST(SubprocessTest, StderrCaptured) {
    Subprocess proc({"echo err >&2"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_EQ(proc.readOutput(), "err\n");
}

TEST(SubprocessTest, InvalidExecutable) {
    Subprocess proc({"__nonexistent_binary_12345__"});
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
    EXPECT_EQ(proc.exitCode(), 127);
}

TEST(SubprocessTest, WorkingDirectory) {
    Subprocess proc({"pwd"}, "/tmp");
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    std::string output = proc.readOutput();
    EXPECT_FALSE(output.empty());
    // macOS: /tmp -> /private/tmp
    EXPECT_TRUE(output.find("tmp") != std::string::npos);
}

TEST(SubprocessTest, Kill) {
    Subprocess proc({"sleep 60"});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(proc.finished());
    proc.kill();
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_TRUE(proc.finished());
    // SIGKILL (signal 9) -> 128 + 9 = 137
    EXPECT_EQ(proc.exitCode(), 137);
}

TEST(SubprocessTest, MultilineOutput) {
    Subprocess proc({"echo line1; echo line2; echo line3"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_EQ(proc.readOutput(), "line1\nline2\nline3\n");
}

TEST(SubprocessTest, ReadOutputBeforeFinished) {
    Subprocess proc({"echo early; sleep 0.2; echo late"});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::string partial = proc.readOutput();
    EXPECT_FALSE(partial.empty());
    proc.join();
    std::string rest = proc.readOutput();
    EXPECT_TRUE(partial.find("early") != std::string::npos || rest.find("early") != std::string::npos);
}

TEST(SubprocessTest, LargeOutput) {
    // Generate ~100KB of output
    Subprocess proc({"dd if=/dev/zero bs=1024 count=100 2>/dev/null | base64"});
    proc.join();
    EXPECT_EQ(proc.exitCode(), 0);
    std::string output = proc.readOutput();
    EXPECT_GT(output.size(), 100000U);
}

TEST(SubprocessTest, StreamingReadWithoutJoin) {
    Subprocess proc({"for i in 1 2 3 4 5; do echo line$i; sleep 0.1; done"});
    std::string all;
    while (!proc.finished()) {
        all += proc.readOutput();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    all += proc.readOutput();
    EXPECT_EQ(proc.exitCode(), 0);
    EXPECT_TRUE(all.find("line1") != std::string::npos);
    EXPECT_TRUE(all.find("line5") != std::string::npos);
    // 验证拿到了所有 5 行
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
    EXPECT_EQ(proc.readOutput(), "first\nsecond\nthird\n");
}

TEST(SubprocessTest, MultipleCommandsStopOnFailure) {
    Subprocess proc({"echo before", "false", "echo after"});
    EXPECT_THROW(proc.join(), std::runtime_error);
    EXPECT_NE(proc.exitCode(), 0);
    std::string output = proc.readOutput();
    EXPECT_TRUE(output.find("before") != std::string::npos);
    EXPECT_TRUE(output.find("after") == std::string::npos);
}

TEST(SubprocessTest, Callback) {
    std::mutex mtx;
    std::string collected;
    auto callback = [&](const std::string& text) {
        std::lock_guard<std::mutex> lock(mtx);
        collected += text;
    };
    Subprocess proc({"echo hello; echo world"}, ".", callback);
    proc.join();
    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_TRUE(collected.find("hello") != std::string::npos);
    EXPECT_TRUE(collected.find("world") != std::string::npos);
}

TEST(SubprocessTest, CallbackStreaming) {
    std::mutex mtx;
    int call_count = 0;
    auto callback = [&](const std::string& /*text*/) {
        std::lock_guard<std::mutex> lock(mtx);
        ++call_count;
    };
    Subprocess proc({"for i in 1 2 3 4 5; do echo line$i; sleep 0.1; done"}, ".", callback);
    proc.join();
    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_GT(call_count, 0);
}

TEST(SubprocessTest, Cancelled) {
    std::atomic<bool> cancelled{false};
    Subprocess proc({"sleep 60"}, ".", nullptr, cancelled);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    Subprocess proc({"for i in $(seq 1 100); do echo line$i; sleep 0.1; done"}, ".", callback, cancelled);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
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
