Build the project and run a full code quality check, executing the following steps in order:

1. Run `cmake --build build` to compile the project (with clang-tidy static analysis)
2. Run `cmake --build build --target check` to verify code formatting compliance
3. Run `ctest --test-dir build --output-on-failure` to execute all unit tests

Report the result of each step. If any step fails, analyze the error and provide specific fix suggestions.
