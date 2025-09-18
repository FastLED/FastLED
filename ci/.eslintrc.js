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
    "no-unused-vars": "error",        // Catch unused variables/imports
    "no-undef": "error",              // Catch undefined variables
    "eqeqeq": "error",                // Require === instead of ==

    // Next top 3 code quality rules
    "no-var": "error",                // Use let/const instead of var
    "prefer-const": "error",          // Use const for variables that never change
    "no-redeclare": "error"           // Prevent variable redeclaration
  }
};