#!/usr/bin/env python3
"""
FastLED JavaScript Type Enhancement and Linting Strategy Script

This script provides multiple approaches for enhancing JavaScript code quality
while keeping files in .js format:

1. Linting Analysis - Analyze current linting issues and provide fixes
2. Type Safety - Add comprehensive JSDoc annotations and type checking
3. Performance - Identify and fix performance issues (await in loops, etc.)
4. Code Quality - Consistent formatting and best practices

Usage:
    python3 scripts/enhance-js-typing.py --approach linting
    python3 scripts/enhance-js-typing.py --approach performance
    python3 scripts/enhance-js-typing.py --approach types --file src/platforms/wasm/compiler/index.js
    python3 scripts/enhance-js-typing.py --approach summary
"""

import argparse
import os
import re
import json
import subprocess
from pathlib import Path
from typing import List, Dict, Tuple, Optional

class JSLintingEnhancer:
    """Comprehensive JavaScript linting and type enhancement tool"""
    
    def __init__(self):
        self.workspace_root = Path.cwd()
        self.wasm_dir = self.workspace_root / "src" / "platforms" / "wasm"
        self.js_files = []
        self._discover_files()
    
    def _discover_files(self):
        """Discover all JavaScript files in the WASM directory"""
        if self.wasm_dir.exists():
            self.js_files = list(self.wasm_dir.rglob("*.js"))
        
    def analyze_current_linting(self) -> Dict:
        """Analyze current linting issues"""
        print("🔍 Analyzing current linting issues...")
        
        try:
            result = subprocess.run(
                [".js-tools/deno/deno", "lint", "--config", "deno.json", "--json"],
                capture_output=True,
                text=True,
                cwd=self.workspace_root
            )
            
            if result.returncode == 0:
                print("✅ No linting issues found!")
                return {"issues": [], "summary": "clean"}
            
            # Parse the output to extract issues
            issues = []
            if result.stdout:
                try:
                    lint_data = json.loads(result.stdout)
                    if isinstance(lint_data, dict) and 'diagnostics' in lint_data:
                        issues = lint_data['diagnostics']
                except json.JSONDecodeError:
                    pass
            
            # If JSON parsing fails, parse text output
            if not issues and result.stdout:
                issues = self._parse_text_lint_output(result.stdout)
            
            return {
                "issues": issues,
                "summary": f"Found {len(issues)} linting issues",
                "raw_output": result.stdout
            }
            
        except Exception as e:
            print(f"❌ Error running linter: {e}")
            return {"issues": [], "summary": "error", "error": str(e)}
    
    def _parse_text_lint_output(self, output: str) -> List[Dict]:
        """Parse text-based lint output into structured format"""
        issues = []
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
                    current_issue = {
                        "rule": rule_name,
                        "message": message,
                        "file": "",
                        "line": 0
                    }
            elif 'at /' in line:
                # Extract file and line info
                match = re.search(r'at\s+([^:]+):(\d+)', line)
                if match and current_issue:
                    current_issue["file"] = match.group(1)
                    current_issue["line"] = int(match.group(2))
                    issues.append(current_issue.copy())
                    current_issue = {}
        
        return issues
    
    def fix_await_in_loop_issues(self) -> List[str]:
        """Identify and provide fixes for await-in-loop issues"""
        print("🔧 Analyzing await-in-loop issues...")
        
        fixes = []
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
        
        def add_jsdoc(match):
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
        
        def add_type_comment(match):
            return f'{match.group(0)} /** @type {{any}} */'
        
        return re.sub(var_pattern, add_type_comment, content)
    
    def _add_class_types(self, content: str) -> str:
        """Add JSDoc types to classes"""
        # Pattern for class declarations
        class_pattern = r'(class\s+(\w+))'
        
        def add_class_jsdoc(match):
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
        """Create different linting configuration variants"""
        print("⚙️  Creating linting configuration variants...")
        
        configs = []
        
        # Strict config
        strict_config = {
            "compilerOptions": {
                "allowJs": True,
                "checkJs": True,
                "strict": True,
                "noImplicitAny": True,
                "strictNullChecks": True,
                "lib": ["dom", "dom.asynciterable", "es2022", "webworker"]
            },
            "lint": {
                "rules": {
                    "tags": ["recommended"],
                    "include": [
                        "eqeqeq", "guard-for-in", "no-await-in-loop", 
                        "no-eval", "prefer-const", "camelcase",
                        "single-var-declarator", "default-param-last"
                    ],
                    "exclude": ["no-console", "no-unused-vars"]
                }
            }
        }
        
        # Gradual config  
        gradual_config = {
            "compilerOptions": {
                "allowJs": True,
                "checkJs": False,
                "strict": False,
                "lib": ["dom", "dom.asynciterable", "es2022", "webworker"]
            },
            "lint": {
                "rules": {
                    "tags": ["recommended"],
                    "include": ["eqeqeq", "no-eval", "prefer-const", "no-await-in-loop"],
                    "exclude": ["no-console", "no-unused-vars", "camelcase", "no-undef"]
                }
            }
        }
        
        # Write config variants
        strict_file = self.workspace_root / "deno.strict.json"
        gradual_file = self.workspace_root / "deno.gradual.json" 
        
        strict_file.write_text(json.dumps(strict_config, indent=2))
        gradual_file.write_text(json.dumps(gradual_config, indent=2))
        
        configs.extend([
            f"✅ Created strict config: {strict_file}",
            f"✅ Created gradual config: {gradual_file}"
        ])
        
        return configs
    
    def run_summary(self) -> str:
        """Generate comprehensive summary of JavaScript codebase"""
        print("📊 Generating comprehensive JavaScript codebase summary...")
        
        summary = []
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
        summary.append(f"  • {lint_result['summary']}")
        
        if lint_result['issues']:
            rule_counts = {}
            for issue in lint_result['issues']:
                rule = issue.get('rule', 'unknown')
                rule_counts[rule] = rule_counts.get(rule, 0) + 1
            
            summary.append(f"  • Most common issues:")
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
        summary.append(f"  • python3 scripts/enhance-js-typing.py --approach linting")
        summary.append(f"  • python3 scripts/enhance-js-typing.py --approach performance") 
        summary.append(f"  • python3 scripts/enhance-js-typing.py --approach types --file <path>")
        summary.append(f"  • .js-tools/deno/deno lint --config deno.json")
        summary.append(f"  • .js-tools/deno/deno check --config deno.json <file>")
        
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
        print(f"  {result['summary']}")
        
        if result['issues']:
            print(f"\n📋 ISSUES BREAKDOWN:")
            for issue in result['issues'][:10]:  # Show first 10
                print(f"  • {issue.get('rule', 'unknown')}: {issue.get('message', 'No message')}")
                if issue.get('file'):
                    print(f"    File: {issue['file']}:{issue.get('line', '?')}")
        
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
