---
name: address-reviews
description: Fetch CodeRabbit review comments on the current PR, classify each finding, apply fixes for valid issues, and reply to each thread. Use before attempting gh pr merge when CodeRabbit has posted a review. Blocks merge on unresolved threads. Routes security-critical findings to a human.
argument-hint: [pr-number, optional — defaults to PR for current branch]
context: fork
---

Address CodeRabbit review comments on a PR end-to-end: fetch, classify, fix, reply, commit, push. See `ci/tools/coderabbit_addressor.py` for the helper that drives the workflow.

Arguments: $ARGUMENTS

## Workflow

1. **Fetch comments**. Run:
   ```bash
   uv run ci/tools/coderabbit_addressor.py $ARGUMENTS --plan
   ```
   This prints a JSON plan listing every unresolved CodeRabbit comment, classified into one of four buckets:
   - `valid-fix` — actionable code change
   - `style` — preference/nit; reply-only
   - `false-positive` — disagree; reply with rationale
   - `security-flag` — flag a human; do not auto-fix

2. **Review the plan**. For each `valid-fix`, read the file/line being flagged and decide the concrete change. For `false-positive`, draft the reply. Do not touch anything in `security-flag` — leave it for a human reviewer.

3. **Apply fixes** one comment at a time. Edit the referenced file, re-read to confirm the change is correct, and stage it.

4. **Reply to each thread** via:
   ```bash
   uv run ci/tools/coderabbit_addressor.py $ARGUMENTS --reply <comment-id> "<message>"
   ```
   Keep replies terse: either "Fixed in <commit-sha-short>" or a one-sentence rationale for disagreement.

5. **Commit and push** using a conventional-commit-format message that lists each addressed comment:
   ```
   fix: address CodeRabbit review feedback

   - <comment-1-summary>
   - <comment-2-summary>
   ```

6. **Re-run the plan**. If `valid-fix` count hits zero and no `security-flag` remains, the PR is ready to merge. Otherwise iterate — hard limit: 3 passes.

## Safety rails

- Max 3 iterations per PR. If CodeRabbit keeps asking for changes after 3 rounds, stop and flag the human reviewer — something structural is off.
- Never auto-fix `security-flag` comments. The classifier routes any comment mentioning `security`, `CVE`, `auth`, `credential`, `secret`, `RCE`, `injection`, `CRITICAL`, `data loss`, `UAF`, `use-after-free`, `buffer overflow`, or `out-of-bounds` into this bucket.
- The pre-merge hook (`ci/hooks/check_pr_merge_reviews.py`) blocks `gh pr merge` until all CodeRabbit threads are resolved, so you cannot accidentally ship with pending feedback.

## When to invoke

- Before any `gh pr merge` on a PR where CodeRabbit has posted review comments.
- After pushing a new commit that might have triggered a re-review — the hook will remind you.

## When NOT to invoke

- On draft PRs where CodeRabbit has not yet reviewed.
- When every open comment is already in a `security-flag` state — defer to a human.
