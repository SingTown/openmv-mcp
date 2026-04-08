Build the project and run a full code quality check, executing the following steps in order:

1. Run `cmake --build build --target format` to auto-format all source files
2. Run `cmake --build build` to compile the project (with clang-tidy static analysis)
3. Run `ctest --test-dir build --output-on-failure` to execute all unit tests
4. Review CLAUDE.md and README.md against the current codebase — verify that commands, module descriptions, and tool lists are accurate and up-to-date. Flag any discrepancies.

Report the result of each step. If any step fails, analyze the error and provide specific fix suggestions.
