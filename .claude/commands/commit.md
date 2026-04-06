Run the full quality assurance pipeline before committing code:

1. Run `/check` to execute the full quality check
2. If all steps pass, analyze all staged and unstaged changes, generate a commit message following the project's commit style, and run `git commit`
3. If any step fails, fix the issue first, then restart from step 1
