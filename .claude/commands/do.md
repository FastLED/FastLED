---
description: Implement task from task.md file with iterative research-plan-implement-test-lint-fix cycles
argument-hint: <path-to-task.md> [max-iterations]
---

Read the task file at $1 and implement it through iterative cycles of read-research-plan-implement-test-lint-fix-update.

Task file: $1
Max iterations: ${2:-1}

Use the 'do-agent' sub-agent to handle this systematically. The sub-agent should track iteration count and stop when complete or if stuck, reporting either DONE! or MORE WORK TO DO.
