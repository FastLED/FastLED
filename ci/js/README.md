# FastLED JavaScript Linting Tools

This directory contains JavaScript linting tools for the FastLED project, using Node.js + ESLint for **fast** JavaScript validation.


### Via Main Lint Script (Recommended)

```bash
# JavaScript linting is included by default (fast only!)
bash lint
```

## Setup

The fast JavaScript linter is automatically installed during project setup:

```bash
# Installs everything including fast JS linting
./install
```

Or manually install just the JavaScript linter:

```bash
uv run ci/setup-js-linting-fast.py
```

This will:
- Download Node.js binary to `.js-tools/node/`
- Install ESLint to `.js-tools/node_modules/`
- Create ESLint configuration in `.js-tools/.eslintrc.js`
- Create fast linting script `.cache/js-tools/lint-js-fast`

## Philosophy: Fast Only

**JavaScript linting follows a "fast only" policy:**
- ✅ **Fast linting available**: Uses Node.js + ESLint (~0.9 seconds)
- ⚠️ **Fast linting unavailable**: Skips JavaScript linting entirely
- ❌ **No slow fallback**: Fast linting or no linting at all

## Configuration

The fast linter only checks for **critical runtime issues**:
- `no-debugger` - Prevents debugger statements in production
- `no-eval` - Prevents eval() security vulnerabilities

**No style enforcement** - focuses on critical issues only for maximum speed.

## Integration

JavaScript linting is included by default in `bash lint` when fast linting is available.

## Performance

- **Fast linting**: ~0.9 seconds (53x faster than previous solution)
- **Total lint time**: ~1.6 seconds (Python + C++ + JavaScript)
