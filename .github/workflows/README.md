# GitHub Actions for FastLED

This directory contains GitHub Actions workflows for the FastLED project.

## Claude Code Integration

### Workflow: `claude-code.yml`

This workflow integrates [Claude Code](https://github.com/anthropics/claude-code-action) to provide AI-powered code assistance directly in pull requests.

#### How it works

1. **Trigger**: When someone comments `@claude` on a pull request
2. **Branch Resolution**: Automatically detects and checks out the correct PR branch (including from forks)
3. **Environment Setup**: Configures Python and uv environment matching the project setup
4. **AI Analysis**: Claude analyzes the PR code and provides assistance

#### Setup Requirements

To use this workflow, you need to configure the following secrets in your repository:

1. **`ANTHROPIC_API_KEY`**: Your Anthropic API key for Claude access
   - Go to Repository Settings → Secrets and Variables → Actions
   - Add a new secret named `ANTHROPIC_API_KEY`
   - Get your API key from [Anthropic Console](https://console.anthropic.com/)

2. **`GITHUB_TOKEN`**: Automatically provided by GitHub (no setup needed)

#### Key Features

**✅ Solves the "Remote Ref Not Found" Issue**

This workflow includes the fix for the common issue where Claude Code Action fails with "fatal: couldn't find remote ref" when triggered from PR comments. The solution:

- Uses `xt0rted/pull-request-comment-branch@v2` to detect PR branch information
- Explicitly checks out the PR's head branch and repository (including forks)
- Provides full git history (`fetch-depth: 0`) for better context

**✅ FastLED Project Integration**

- Automatically sets up Python 3.11 and uv package manager
- Runs `uv sync` to install project dependencies
- Compatible with the project's Python build system and MCP server tools

**✅ Secure Fork Support**

- Works with pull requests from forked repositories
- Uses GitHub's standard token permissions for read access
- Maintains security by running in the base repository's context

#### Usage

1. Create a pull request or comment on an existing one
2. Mention `@claude` in your comment with your request
3. The workflow will automatically trigger and Claude will respond

Example comments:
```
@claude please review this code for potential optimizations

@claude can you help fix the compilation errors in the ESP32 platform code?

@claude suggest improvements to the WASM bindings
```

#### Troubleshooting

**"Anthropic API key not found"**
- Ensure `ANTHROPIC_API_KEY` is set in repository secrets
- Verify the secret name matches exactly (case-sensitive)

**"Permission denied" or checkout failures**
- The workflow uses standard GitHub permissions
- For private repositories, ensure the token has appropriate access
- Fork PRs from private repos may have limited access for security

**Workflow not triggering**
- Ensure the comment contains `@claude` exactly
- The workflow only runs on pull request comments, not issue comments
- Check that Actions are enabled in the repository settings

#### Technical Details

**Why this approach works:**

Issue comment events in GitHub Actions run in the context of the base branch, not the PR branch. This causes the standard checkout to fetch the wrong code. Our solution:

1. **PR Detection**: Uses the specialized `xt0rted/pull-request-comment-branch` action
2. **Explicit Checkout**: Fetches the exact PR branch and repository
3. **Context Preservation**: Maintains full git history for Claude's analysis

This approach is based on [community solutions](https://stackoverflow.com/questions/65962371) and ensures Claude always has access to the actual PR changes.

#### Maintenance

- Review the `anthropics/claude-code-action` releases for updates
- Update the action version (`@beta` → specific version) when stable
- Monitor the `xt0rted/pull-request-comment-branch` action for updates
- Adjust permissions if Claude needs additional repository access