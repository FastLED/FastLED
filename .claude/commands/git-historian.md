---
description: Search codebase and git history for keywords
argument-hint: <keyword1> [keyword2] [...] [--paths dir1 dir2]
---

Search the codebase and git history for the specified keywords.

Arguments: ${1:-} ${2:-} ${3:-} ${4:-} ${5:-} ${6:-}

This command uses the git_historian MCP tool to search both:
1. Working tree files (current code state)
2. Git history (last 10 commits' diffs)

Usage examples:
- /git-historian LED strip
- /git-historian "error handling" config
- /git-historian memory leak --paths src tests

The tool returns compact contexts combining:
- File locations with matching lines
- Historical commits showing changes related to keywords

Results are optimized for AI consumption and complete in under 4 seconds.

Extract the keywords from the arguments and call the git_historian MCP tool with those keywords.
If --paths is specified, extract the paths that follow it and pass them as the paths parameter.
Otherwise, pass an empty paths array to search the entire repository.

Present the results in a clear, readable format showing both current code locations and historical context.
