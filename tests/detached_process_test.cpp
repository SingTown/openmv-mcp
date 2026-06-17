#include "detached_process.h"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <string>

TEST(DetachedProcessTest, MissingExecutableThrowsPromptly) {
    auto start = std::chrono::steady_clock::now();

    try {
        mcp::ensureServerRunning("openmv_mcp_missing_executable_for_test", 19057, "", "");
        FAIL() << "expected missing executable to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("failed"), std::string::npos);
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, std::chrono::seconds(2));
}
