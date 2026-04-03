Run the full quality assurance pipeline before committing code:

1. Run `cmake --build build --target format` to auto-format all source files
2. Run `cmake --build build` to compile the project (with clang-tidy static analysis)
3. Run `ctest --test-dir build --output-on-failure` to execute all unit tests
4. If all steps pass, analyze all staged and unstaged changes, generate a commit message following the project's commit style, and run `git commit`
5. If any step fails, fix the issue first, then restart from step 1
