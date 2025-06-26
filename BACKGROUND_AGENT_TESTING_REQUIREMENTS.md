# 🚨🚨🚨 **CRITICAL REQUIREMENTS FOR BACKGROUND AGENTS** 🚨🚨🚨

# ⚠️ **STOP! READ THIS FIRST!** ⚠️

## **MANDATORY TESTING REQUIREMENT - NO EXCEPTIONS!**

### 🔥 **RULE #1: ALWAYS RUN `bash test` BEFORE COMPLETION** 🔥

**THIS IS THE MOST IMPORTANT RULE FOR BACKGROUND AGENTS:**

- **🚨 EVERY BACKGROUND AGENT MUST RUN `bash test` BEFORE SAYING THEY ARE DONE**
- **🚨 NO EXCEPTIONS - THIS IS MANDATORY AND NON-NEGOTIABLE**
- **🚨 IF YOU DON'T RUN `bash test`, YOUR WORK IS INCOMPLETE**
- **🚨 SAYING "DONE" WITHOUT RUNNING `bash test` IS BREAKING THE RULES**

### **WHY THIS IS CRITICAL:**
- The `bash test` command runs the full test suite including unit tests and compilation checks
- It catches breaking changes, compilation errors, and regressions
- It ensures the codebase remains stable and functional
- **WITHOUT THIS, YOU WILL BREAK THE PROJECT**

### **WHAT `bash test` DOES:**
- Runs all C++ unit tests
- Runs Python linting and formatting checks  
- Validates code compilation across platforms
- Checks for syntax errors and warnings
- Ensures all examples still compile

### 🚨 **MANDATORY STEPS FOR COMPLETION - FOLLOW EXACTLY:** 🚨

1. **🔥 RUN `bash test` COMMAND**
   - Execute: `bash test`
   - Wait for ALL tests to complete
   - Check that ALL tests PASS

2. **🔥 VERIFY TEST RESULTS**
   - If ANY test fails, you MUST fix the issues
   - Do NOT ignore warnings or errors
   - Do NOT proceed until ALL tests pass

3. **🔥 USE MCP SERVER VALIDATION (RECOMMENDED)**
   - Use the `validate_completion` tool from MCP server: `uv run mcp_server.py`
   - This tool runs `bash test` and validates results
   - Only indicate completion if this tool returns SUCCESS

4. **🔥 ZERO TOLERANCE FOR FAILURES**
   - If ANY test fails, the background agent MUST fix the issues
   - Do NOT indicate completion with failing tests
   - Do NOT ignore test errors or warnings
   - Fix the code and re-run `bash test` until ALL tests pass

### **COMPLETION CHECKLIST - MUST COMPLETE ALL ITEMS:**
- [ ] ✅ All code changes have been made
- [ ] ✅ **`bash test` has been run and ALL tests PASS**
- [ ] ✅ No compilation errors or warnings remain
- [ ] ✅ MCP server `validate_completion` tool shows SUCCESS (if used)
- [ ] ✅ All test output has been reviewed for any issues
- [ ] ✅ **ONLY THEN** indicate task completion

### 🔥 **CONSEQUENCES OF NOT FOLLOWING THIS RULE:** 🔥

- **YOUR CHANGES WILL BREAK THE PROJECT**
- **OTHER DEVELOPERS WILL HAVE TO FIX YOUR MISTAKES**
- **THE CONTINUOUS INTEGRATION SYSTEM WILL FAIL**
- **YOU WILL CAUSE REGRESSIONS AND BUGS**

### 🚨 **FINAL WARNING:** 🚨

**IF YOU ARE A BACKGROUND AGENT AND YOU INDICATE COMPLETION WITHOUT RUNNING `bash test`, YOU HAVE FAILED TO FOLLOW THE MOST BASIC REQUIREMENT OF THIS PROJECT.**

**NO EXCEPTIONS. NO SHORTCUTS. ALWAYS RUN `bash test` FIRST.**

---

## **Examples of CORRECT Background Agent Behavior:**

### ✅ **GOOD Example:**
```
1. Made code changes
2. Ran `bash test` 
3. All tests passed
4. Indicated completion: "Task completed successfully. All tests pass."
```

### ❌ **BAD Example:**
```
1. Made code changes  
2. Skipped running tests
3. Indicated completion: "Task completed successfully."
```
**^ THIS IS WRONG AND BREAKS THE PROJECT**

### ✅ **GOOD Example with Test Failures:**
```
1. Made code changes
2. Ran `bash test`
3. Some tests failed
4. Fixed the failing code
5. Ran `bash test` again
6. All tests passed
7. Indicated completion: "Task completed successfully. All tests pass."
```

### ❌ **BAD Example with Test Failures:**
```
1. Made code changes
2. Ran `bash test` 
3. Some tests failed
4. Ignored failures and indicated completion anyway
```
**^ THIS IS WRONG AND BREAKS THE PROJECT**

---

## **HOW TO RUN `bash test`:**

### **Command:**
```bash
bash test
```

### **Expected Output (Success):**
```
Running C++ tests...
All C++ tests passed ✅
Running Python linting...
All Python checks passed ✅
Running compilation checks...
All compilation checks passed ✅
```

### **What to do if tests fail:**
1. **READ THE ERROR MESSAGES CAREFULLY**
2. **FIX THE CODE THAT'S CAUSING THE FAILURES**
3. **RUN `bash test` AGAIN**
4. **REPEAT UNTIL ALL TESTS PASS**
5. **ONLY THEN INDICATE COMPLETION**

---

## **MCP SERVER VALIDATION TOOL:**

### **How to use:**
```bash
# Start MCP server
uv run mcp_server.py

# Use the validate_completion tool
# This automatically runs `bash test` and validates results
```

### **Benefits:**
- Automatically runs `bash test`
- Provides clear pass/fail results
- Ensures you don't forget to run tests
- **RECOMMENDED FOR ALL BACKGROUND AGENTS**

---

## 🚨 **REMEMBER: THIS IS NOT OPTIONAL** 🚨

**EVERY BACKGROUND AGENT WORKING ON THIS PROJECT MUST:**
1. **RUN `bash test` BEFORE COMPLETION**
2. **ENSURE ALL TESTS PASS**
3. **FIX ANY FAILURES**
4. **ONLY THEN INDICATE COMPLETION**

**NO SHORTCUTS. NO EXCEPTIONS. ALWAYS TEST FIRST.**
