# FastLED JavaScript Linting Tools

This directory contains JavaScript linting tools for the FastLED project, using Deno for fast and simple JavaScript validation.

## Scripts

- **`lint-js`** - Basic JavaScript linting using Deno lint
- **`check-js`** - Enhanced linting with optional type checking via JSDoc
- **`format-js`** - JavaScript code formatting using Deno fmt
- **`lint-js-minimal`** - Speed-optimized minimal linting (legacy)
- **`lint-js-instant`** - Fastest possible syntax validation (legacy)

## Usage

### From Project Root

```bash
# Basic linting
ci/js/lint-js

# Enhanced linting + type checking
ci/js/check-js

# Format JavaScript files
ci/js/format-js
```

### Via Main Lint Script (Recommended)

```bash
# JavaScript linting is included by default (it's fast!)
bash lint
```

## Setup

Run the setup script to install Deno and create all necessary files:

```bash
uv run ci/setup-js-linting.py
```

This will:
- Download Deno binary to `.js-tools/deno/`
- Create `deno.json` configuration in project root
- Create all linting scripts in `ci/js/`
- Update `.gitignore` to exclude Deno tools

## Integration

JavaScript linting is now included by default in `bash lint` because it's fast and lightweight. No need for the old `--js` flag!

## Files

All scripts work from the `ci/js/` directory but operate on the project root via `cd ../../`.

The Deno configuration (`deno.json`) remains in the project root for consistency with Deno conventions.
