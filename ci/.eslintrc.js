module.exports = {
  env: {
    browser: true,
    es2022: true,
    worker: true
  },
  parserOptions: {
    ecmaVersion: 2022,
    sourceType: "module"
  },
  rules: {
    // FAST LINTING: Only check for critical runtime issues
    "no-debugger": "error",
    "no-eval": "error",

    // Essential quality rules
    "no-unused-vars": ["error", { "argsIgnorePattern": "^_" }], // Catch unused variables/imports, ignore underscore-prefixed
    "no-undef": "error",              // Catch undefined variables
    "eqeqeq": "error",                // Require === instead of ==

    // Next top 3 code quality rules
    "no-var": "error",                // Use let/const instead of var
    "prefer-const": "error",          // Use const for variables that never change
    "no-redeclare": "error",          // Prevent variable redeclaration

    // Additional rules based on WASM code analysis
    "radix": "error",                 // Require radix parameter for parseInt()
    "no-magic-numbers": "off",        // Allow magic numbers
    "complexity": ["warn", 35],       // Warn on high function complexity (increased for complex graphics/UI code)
    "max-len": ["warn", {             // Limit line length for readability
      "code": 140,                    // Increased for complex expressions
      "ignoreUrls": true,
      "ignoreStrings": true,
      "ignoreTemplateLiterals": true,
      "ignoreComments": true
    }],
    "no-console": "off",              // Allow console for this debugging-heavy codebase
    "prefer-template": "warn",        // Prefer template literals over string concatenation
    "no-implicit-coercion": "off",    // Disable - DOM existence checks are common and clear
    "require-await": "off",           // Disable - async API consistency is more important
    "no-await-in-loop": "off",        // Disable - necessary for audio processing and sequential operations
    "no-promise-executor-return": "error", // Prevent return in Promise executor
    "prefer-promise-reject-errors": "error", // Require Error objects for Promise.reject
    "no-unreachable": "error",        // Catch unreachable code
    "no-empty-function": "off",       // Allow empty functions (common for stubs)
    "consistent-return": "warn"       // Warn about inconsistent return statements
  }
};