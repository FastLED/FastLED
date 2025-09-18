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

    // Essential quality rules (3 new additions)
    "no-unused-vars": "error",        // Catch unused variables/imports
    "no-undef": "error",              // Catch undefined variables
    "eqeqeq": "error"                 // Require === instead of ==
  }
};