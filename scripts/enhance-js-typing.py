#!/usr/bin/env python3
"""
FastLED JavaScript Type Enhancement and Linting Strategy Script

This script provides multiple approaches for enhancing JavaScript code quality
while keeping files in .js format. Uses fast Node.js + ESLint for linting.

1. Linting Analysis - Analyze current ESLint issues and provide fixes
2. Type Safety - Add comprehensive JSDoc annotations and type checking
3. Performance - Identify and fix performance issues (await in loops, etc.)
4. Code Quality - Consistent formatting and best practices
5. Config Management - Create ESLint configuration variants

Usage:
    uv run scripts/enhance-js-typing.py --approach linting
    uv run scripts/enhance-js-typing.py --approach performance
    uv run scripts/enhance-js-typing.py --approach types --file src/platforms/wasm/compiler/index.js
    uv run scripts/enhance-js-typing.py --approach configs
    uv run scripts/enhance-js-typing.py --approach summary
"""

import argparse
import re
import json
import subprocess
from pathlib import Path
from typing import List, Dict, Match, TypedDict, Union

class LintIssue(TypedDict):
    """Type definition for a linting issue"""
    rule: str
    message: str
    file: str
    line: int
    severity: int

class LintResult(TypedDict, total=False):
    """Type definition for lint analysis result"""
    issues: List[LintIssue]
    summary: str
    raw_output: str
    error: str

class JSLintingEnhancer:
    """Comprehensive JavaScript linting and type enhancement tool"""
    
    def __init__(self) -> None:
        self.workspace_root: Path = Path.cwd()
        self.wasm_dir: Path = self.workspace_root / "src" / "platforms" / "wasm"
        self.js_files: List[Path] = []
        self._discover_files()
    
    def _discover_files(self) -> None:
        """Discover all JavaScript files in the WASM directory"""
        if self.wasm_dir.exists():
            self.js_files = list(self.wasm_dir.rglob("*.js"))
        
    def analyze_current_linting(self) -> LintResult:
        """Analyze current linting issues using fast ESLint"""
        print("🔍 Analyzing current linting issues...")
        
        try:
            # Check if fast linting is available
            import platform
            eslint_exe = ".cache/js-tools/node_modules/.bin/eslint.cmd" if platform.system() == "Windows" else ".cache/js-tools/node_modules/.bin/eslint"
            
            if not Path(eslint_exe).exists():
                return LintResult(issues=[], summary="Fast linting not available. Run: uv run ci/setup-js-linting-fast.py", error="eslint_not_found")
            
            # Run ESLint with JSON output
            result = subprocess.run(
                [eslint_exe, "--format", "json", "--no-eslintrc", "--no-inline-config", "-c", ".cache/js-tools/.eslintrc.js", 
                 "src/platforms/wasm/compiler/*.js", "src/platforms/wasm/compiler/modules/*.js"],
                capture_output=True,
                text=True,
                cwd=self.workspace_root,
                shell=platform.system() == "Windows"
            )
            
            if result.returncode == 0:
                print("✅ No linting issues found!")
                return LintResult(issues=[], summary="clean", raw_output="", error="")
            
            # Parse ESLint JSON output
            issues: List[LintIssue] = []
            if result.stdout:
                try:
                    lint_data = json.loads(result.stdout)
                    for file_result in lint_data:
                        for message in file_result.get('messages', []):
                            issues.append(LintIssue(
                                rule=message.get('ruleId', 'unknown'),
                                message=message.get('message', ''),
                                file=file_result.get('filePath', ''),
                                line=message.get('line', 0),
                                severity=message.get('severity', 1)
                            ))
                except json.JSONDecodeError:
                    pass
            
            return LintResult(
                issues=issues,
                summary=f"Found {len(issues)} linting issues",
                raw_output=result.stdout,
                error=""
            )
        except Exception as e:
            print(f"❌ Error running linter: {e}")
            return LintResult(issues=[], summary="error", raw_output="", error=str(e))
    
    def _parse_text_lint_output(self, output: str) -> List[LintIssue]:
        """Parse text-based lint output into structured format"""
        issues: List[LintIssue] = []
        lines = output.split('\n')
        
        current_issue = {}
        for line in lines:
            line = line.strip()
            if not line:
                continue
                
            # Look for rule violations
            if line.startswith('(') and ')' in line:
                rule_match = re.match(r'\(([^)]+)\)\s*(.*)', line)
                if rule_match:
                    rule_name = rule_match.group(1)
                    message = rule_match.group(2)
                    current_issue: Dict[str, Union[str, int]] = {
                        "rule": rule_name,
                        "message": message,
                        "file": "",
                        "line": 0,
                        "severity": 1
                    }
            elif 'at /' in line:
                # Extract file and line info
                match = re.search(r'at\s+([^:]+):(\d+)', line)
                if match and current_issue:
                    issues.append(LintIssue(
                        rule=str(current_issue["rule"]),
                        message=str(current_issue["message"]),
                        file=match.group(1),
                        line=int(match.group(2)),
                        severity=1
                    ))
                    current_issue = {}
        
        return issues
    
    def fix_await_in_loop_issues(self) -> List[Dict[str, Union[str, int]]]:
        """Identify and provide fixes for await-in-loop issues"""
        print("🔧 Analyzing await-in-loop issues...")
        
        fixes: List[Dict[str, Union[str, int]]] = []
        for js_file in self.js_files:
            try:
                content = js_file.read_text()
                lines = content.split('\n')
                
                for i, line in enumerate(lines, 1):
                    if 'await' in line and ('for' in line or 'while' in line):
                        # Simple heuristic for await in loop
                        context = '\n'.join(lines[max(0, i-3):i+2])
                        fixes.append({
                            "file": str(js_file.relative_to(self.workspace_root)),
                            "line": i,
                            "issue": "await in loop",
                            "context": context,
                            "suggestion": "Consider using Promise.all() for parallel execution"
                        })
            
            except Exception as e:
                print(f"Warning: Could not analyze {js_file}: {e}")
        
        return fixes
    
    def enhance_jsdoc_types(self, file_path: str) -> str:
        """Add comprehensive JSDoc type annotations to a file"""
        target_file = Path(file_path)
        if not target_file.exists():
            return f"❌ File not found: {file_path}"
        
        try:
            content = target_file.read_text()
            
            # Add type annotations for common patterns
            enhanced_content = self._add_function_types(content)
            enhanced_content = self._add_variable_types(enhanced_content)
            enhanced_content = self._add_class_types(enhanced_content)
            
            # Write enhanced content
            backup_file = target_file.with_suffix('.js.bak')
            target_file.rename(backup_file)
            target_file.write_text(enhanced_content)
            
            return f"✅ Enhanced {file_path} with JSDoc types (backup: {backup_file})"
            
        except Exception as e:
            return f"❌ Error enhancing {file_path}: {e}"
    
    def _add_function_types(self, content: str) -> str:
        """Add JSDoc types to functions"""
        # Pattern for function declarations
        function_pattern = r'(function\s+(\w+)\s*\([^)]*\)\s*\{)'
        
        def add_jsdoc(match: Match[str]) -> str:
            func_name = match.group(2)
            return f'''/**
 * {func_name} function
 * @param {{any}} ...args - Function arguments  
 * @returns {{any}} Function result
 */
{match.group(1)}'''
        
        return re.sub(function_pattern, add_jsdoc, content)
    
    def _add_variable_types(self, content: str) -> str:
        """Add JSDoc types to variables"""
        # Pattern for const/let declarations
        var_pattern = r'(const|let)\s+(\w+)\s*='
        
        def add_type_comment(match: Match[str]) -> str:
            return f'{match.group(0)} /** @type {{any}} */'
        
        return re.sub(var_pattern, add_type_comment, content)
    
    def _add_class_types(self, content: str) -> str:
        """Add JSDoc types to classes"""
        # Pattern for class declarations
        class_pattern = r'(class\s+(\w+))'
        
        def add_class_jsdoc(match: Match[str]) -> str:
            class_name = match.group(2)
            return f'''/**
 * {class_name} class
 * @class
 */
{match.group(1)}'''
        
        return re.sub(class_pattern, add_class_jsdoc, content)
    
    def generate_type_definitions(self) -> str:
        """Generate enhanced type definitions"""
        print("📝 Generating enhanced type definitions...")
        
        types_content = '''/**
 * Enhanced FastLED WASM Type Definitions
 * Auto-generated comprehensive types for improved type safety
 */

// Browser Environment Extensions
declare global {
  interface Window {
    audioData?: {
      audioBuffers: Record<string, AudioBufferStorage>;
      hasActiveSamples: boolean;
    };
    
    // FastLED global functions
    FastLED_onFrame?: (frameData: any, callback: Function) => void;
    FastLED_onStripUpdate?: (data: any) => void;
    FastLED_onStripAdded?: (id: number, length: number) => void;
    FastLED_onUiElementsAdded?: (data: any) => void;
  }
  
  // Audio API types
  interface AudioWorkletProcessor {
    process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean;
  }
  
  // WebAssembly Module types
  interface WASMModule {
    _malloc(size: number): number;
    _free(ptr: number): void;
    _extern_setup?: () => void;
    _extern_loop?: () => void;
    ccall(name: string, returnType: string, argTypes: string[], args: any[]): any;
  }
}

// FastLED specific interfaces
interface FastLEDConfig {
  canvasId: string;
  uiControlsId: string;
  outputId: string;
  frameRate?: number;
}

interface StripData {
  strip_id: number;
  pixel_data: Uint8Array;
  length: number;
  diameter?: number;
}

interface ScreenMapData {
  strips: Record<string, Array<{ x: number; y: number }>>;
  absMin: [number, number];
  absMax: [number, number];
}

export {};
'''
        
        types_file = self.workspace_root / "src" / "platforms" / "wasm" / "types.enhanced.d.ts"
        types_file.write_text(types_content)
        
        return f"✅ Generated enhanced types: {types_file}"
    
    def create_linting_config_variants(self) -> List[str]:
        """Create different ESLint configuration variants and return success messages"""
        print("⚙️  Creating ESLint configuration variants...")
        
        configs: List[str] = []
        
        # Strict ESLint config
        strict_config = '''module.exports = {
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
    // Critical issues
    "no-debugger": "error",
    "no-eval": "error",
    // Code quality
    "eqeqeq": "error",
    "prefer-const": "error",
    "no-var": "error",
    "no-await-in-loop": "error",
    "guard-for-in": "error",
    "camelcase": "warn",
    "default-param-last": "warn"
  }
};'''
        
        # Minimal ESLint config  
        minimal_config = '''module.exports = {
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
    // Only critical runtime issues
    "no-debugger": "error",
    "no-eval": "error"
  }
};'''
        
        # Write config variants
        strict_file = self.workspace_root / ".cache/js-tools" / ".eslintrc.strict.js"
        minimal_file = self.workspace_root / ".cache/js-tools" / ".eslintrc.minimal.js"
        
        # Ensure .cache/js-tools directory exists
        (self.workspace_root / ".cache/js-tools").mkdir(parents=True, exist_ok=True)
        
        strict_file.write_text(strict_config)
        minimal_file.write_text(minimal_config)
        
        configs.extend([
            f"✅ Created strict ESLint config: {strict_file}",
            f"✅ Created minimal ESLint config: {minimal_file}"
        ])
        
        return configs
    
    def run_summary(self) -> str:
        """Generate comprehensive summary of JavaScript codebase and return formatted report"""
        print("📊 Generating comprehensive JavaScript codebase summary...")
        
        summary: List[str] = []
        summary.append("=" * 80)
        summary.append("FASTLED JAVASCRIPT LINTING & TYPE SAFETY SUMMARY")
        summary.append("=" * 80)
        
        # File statistics
        total_lines = 0
        total_files = len(self.js_files)
        
        for js_file in self.js_files:
            try:
                lines = len(js_file.read_text().split('\n'))
                total_lines += lines
            except:
                pass
        
        summary.append(f"\n📁 CODEBASE OVERVIEW:")
        summary.append(f"  • Total JavaScript files: {total_files}")
        summary.append(f"  • Total lines of code: {total_lines:,}")
        summary.append(f"  • Average file size: {total_lines // max(total_files, 1):,} lines")
        
        # Current linting status
        lint_result = self.analyze_current_linting()
        summary.append(f"\n🔍 CURRENT LINTING STATUS:")
        summary.append(f"  • {lint_result.get('summary', 'No summary available')}")
        
        issues = lint_result.get('issues', [])
        if issues:
            rule_counts: Dict[str, int] = {}
            for issue in issues:
                rule = issue.get('rule', 'unknown')
                rule_counts[rule] = rule_counts.get(rule, 0) + 1
            
            summary.append(f"  • Most common issues:")
            # Sort by count
            for rule, count in sorted(rule_counts.items(), key=lambda x: x[1], reverse=True)[:5]:
                summary.append(f"    - {rule}: {count} occurrences")
        
        # Recommendations
        summary.append(f"\n🎯 RECOMMENDED NEXT STEPS:")
        summary.append(f"  1. Fix critical performance issues (await-in-loop)")
        summary.append(f"  2. Gradually enable stricter type checking per file")
        summary.append(f"  3. Add comprehensive JSDoc annotations")
        summary.append(f"  4. Enable additional linting rules incrementally")
        summary.append(f"  5. Create automated type checking CI pipeline")
        
        # Available tools
        summary.append(f"\n🛠️  AVAILABLE ENHANCEMENT TOOLS:")
        summary.append(f"  • uv run scripts/enhance-js-typing.py --approach linting")
        summary.append(f"  • uv run scripts/enhance-js-typing.py --approach performance") 
        summary.append(f"  • uv run scripts/enhance-js-typing.py --approach types --file <path>")
        summary.append(f"  • uv run scripts/enhance-js-typing.py --approach configs")
        summary.append(f"  • bash .cache/js-tools/lint-js-fast    # Fast ESLint linting")
        summary.append(f"  • bash lint                  # Full project linting")
        
        summary.append("\n" + "=" * 80)
        
        return "\n".join(summary)

def main():
    parser = argparse.ArgumentParser(description="FastLED JavaScript Enhancement Tool")
    parser.add_argument("--approach", choices=["summary", "linting", "performance", "types", "configs"], 
                       default="summary", help="Enhancement approach")
    parser.add_argument("--file", help="Specific file to enhance (for types approach)")
    
    args = parser.parse_args()
    enhancer = JSLintingEnhancer()
    
    if args.approach == "summary":
        print(enhancer.run_summary())
    
    elif args.approach == "linting":
        result = enhancer.analyze_current_linting()
        print(f"\n🔍 LINTING ANALYSIS:")
        print(f"  {result.get('summary', 'No summary available')}")
        
        issues = result.get('issues', [])
        if issues:
            print(f"\n📋 ISSUES BREAKDOWN:")
            for issue in issues[:10]:  # Show first 10
                print(f"  • {issue.get('rule', 'unknown')}: {issue.get('message', 'No message')}")
                if issue.get('file'):
                    print(f"    File: {issue.get('file', '')}:{issue.get('line', '?')}")
        
    elif args.approach == "performance":
        fixes = enhancer.fix_await_in_loop_issues()
        print(f"\n⚡ PERFORMANCE ANALYSIS:")
        print(f"  Found {len(fixes)} potential await-in-loop issues")
        
        for fix in fixes[:5]:  # Show first 5
            print(f"\n  📁 {fix['file']}:{fix['line']}")
            print(f"     Issue: {fix['issue']}")
            print(f"     Suggestion: {fix['suggestion']}")
    
    elif args.approach == "types":
        if not args.file:
            print("❌ --file argument required for types approach")
            return
        
        result = enhancer.enhance_jsdoc_types(args.file)
        print(result)
        
        # Also generate enhanced type definitions
        type_result = enhancer.generate_type_definitions()
        print(type_result)
    
    elif args.approach == "configs":
        configs = enhancer.create_linting_config_variants()
        for config in configs:
            print(config)

if __name__ == "__main__":
    main()
