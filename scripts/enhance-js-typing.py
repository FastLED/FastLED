#!/usr/bin/env python3
"""
FastLED JavaScript Type Enhancement Script

This script provides multiple approaches for adding strict type checking to 
JavaScript files while keeping them in .js format:

1. JSDoc Enhancement - Add comprehensive JSDoc annotations
2. @ts-check Comments - Enable TypeScript checking per-file
3. Assertion Comments - Add runtime type assertions
4. Flow Comments - Add Flow type annotations in comments

Usage:
    python3 scripts/enhance-js-typing.py --approach jsdoc --file src/platforms/wasm/compiler/index.js
    python3 scripts/enhance-js-typing.py --approach ts-check --all
    python3 scripts/enhance-js-typing.py --approach assertions --file audio_manager.js
"""

import argparse
import os
import re
import json
from pathlib import Path
from typing import List, Dict, Optional, Tuple

class JSTypeEnhancer:
    """Enhances JavaScript files with various typing approaches"""
    
    WASM_JS_DIR = Path("src/platforms/wasm")
    
    def __init__(self):
        self.approaches = {
            'jsdoc': self.enhance_with_jsdoc,
            'ts-check': self.enhance_with_ts_check,
            'assertions': self.enhance_with_assertions,
            'flow': self.enhance_with_flow,
            'summary': self.show_enhancement_summary
        }
    
    def find_js_files(self) -> List[Path]:
        """Find all JavaScript files in the WASM platform directory"""
        js_files = []
        for root, dirs, files in os.walk(self.WASM_JS_DIR):
            for file in files:
                if file.endswith('.js'):
                    js_files.append(Path(root) / file)
        return js_files
    
    def analyze_file_complexity(self, file_path: Path) -> Dict:
        """Analyze a JavaScript file to determine enhancement complexity"""
        try:
            content = file_path.read_text(encoding='utf-8')
        except Exception as e:
            return {'error': str(e)}
        
        # Count various code elements
        functions = len(re.findall(r'function\s+\w+|=>\s*{|:\s*function', content))
        classes = len(re.findall(r'class\s+\w+|export\s+class\s+\w+', content))
        params = len(re.findall(r'function\s*\([^)]*\)|=>\s*\([^)]*\)', content))
        jsdoc_exists = len(re.findall(r'/\*\*.*?\*/', content, re.DOTALL))
        
        return {
            'lines': len(content.splitlines()),
            'functions': functions,
            'classes': classes,
            'parameters': params,
            'existing_jsdoc': jsdoc_exists,
            'has_ts_check': '@ts-check' in content,
            'complexity_score': functions + classes + (params / 5),
            'jsdoc_coverage': jsdoc_exists / max(functions, 1) * 100
        }
    
    def enhance_with_jsdoc(self, file_path: Path, dry_run: bool = True) -> str:
        """Enhance file with comprehensive JSDoc annotations"""
        analysis = self.analyze_file_complexity(file_path)
        
        recommendations = []
        
        if analysis.get('jsdoc_coverage', 0) < 50:
            recommendations.append("🔧 ADD: Comprehensive JSDoc for functions and classes")
            recommendations.append("   Example:")
            recommendations.append("   /**")
            recommendations.append("    * @param {string} name - Parameter description")
            recommendations.append("    * @returns {Promise<Object>} Description")
            recommendations.append("    */")
        
        if analysis.get('parameters', 0) > 10:
            recommendations.append("🔧 ADD: @typedef for complex parameter objects")
            recommendations.append("   Example:")
            recommendations.append("   /**")
            recommendations.append("    * @typedef {Object} ConfigOptions") 
            recommendations.append("    * @property {string} canvasId - Canvas element ID")
            recommendations.append("    * @property {boolean} debug - Enable debug mode")
            recommendations.append("    */")
        
        return '\n'.join(recommendations)
    
    def enhance_with_ts_check(self, file_path: Path, dry_run: bool = True) -> str:
        """Add @ts-check comment to enable TypeScript checking"""
        try:
            content = file_path.read_text(encoding='utf-8')
        except Exception as e:
            return f"❌ Error reading file: {e}"
        
        if '@ts-check' in content:
            return "✅ File already has @ts-check enabled"
        
        recommendations = []
        recommendations.append("🔧 ADD: @ts-check comment at top of file")
        recommendations.append("   Add this line after existing comments:")
        recommendations.append("   // @ts-check")
        recommendations.append("")
        
        if not dry_run:
            # Find insertion point (after existing eslint comments)
            lines = content.splitlines()
            insert_index = 0
            
            # Skip past eslint disable comments
            for i, line in enumerate(lines):
                if line.strip().startswith('/* eslint-disable') or line.strip().startswith('//'):
                    insert_index = i + 1
                elif line.strip() and not line.strip().startswith('/*') and not line.strip().startswith('//'):
                    break
            
            lines.insert(insert_index, '// @ts-check')
            
            enhanced_content = '\n'.join(lines)
            file_path.write_text(enhanced_content, encoding='utf-8')
            recommendations.append("✅ Added @ts-check to file")
        
        return '\n'.join(recommendations)
    
    def enhance_with_assertions(self, file_path: Path, dry_run: bool = True) -> str:
        """Add runtime type assertion comments"""
        recommendations = []
        recommendations.append("🔧 ADD: Runtime type assertions using assert comments")
        recommendations.append("   Examples:")
        recommendations.append("   // @assert {HTMLCanvasElement} canvas")
        recommendations.append("   // @assert {WebGLRenderingContext} gl")
        recommendations.append("   // @assert {StripData[]} frameData")
        recommendations.append("")
        recommendations.append("💡 These can be processed by custom tooling for runtime checks")
        
        return '\n'.join(recommendations)
    
    def enhance_with_flow(self, file_path: Path, dry_run: bool = True) -> str:
        """Add Flow type annotations in comments"""
        recommendations = []
        recommendations.append("🔧 ADD: Flow type annotations in comments")
        recommendations.append("   Examples:")
        recommendations.append("   // @flow")
        recommendations.append("   // type StripData = {strip_id: number, pixel_data: Uint8Array}")
        recommendations.append("   // function updateDisplay(frameData /*: StripData[] */) {")
        recommendations.append("")
        recommendations.append("⚠️  Requires Flow tooling setup")
        
        return '\n'.join(recommendations)
    
    def show_enhancement_summary(self, file_path: Path = None, dry_run: bool = True) -> str:
        """Show summary of all enhancement approaches"""
        if file_path:
            files = [file_path]
        else:
            files = self.find_js_files()
        
        summary = []
        summary.append("📊 FastLED JavaScript Type Enhancement Summary")
        summary.append("=" * 50)
        summary.append("")
        
        total_lines = 0
        total_functions = 0
        files_with_jsdoc = 0
        files_with_ts_check = 0
        
        for file in files:
            analysis = self.analyze_file_complexity(file)
            if 'error' in analysis:
                continue
                
            total_lines += analysis['lines']
            total_functions += analysis['functions']
            
            if analysis['existing_jsdoc'] > 0:
                files_with_jsdoc += 1
            if analysis['has_ts_check']:
                files_with_ts_check += 1
            
            try:
                rel_path = file.relative_to(Path.cwd())
            except ValueError:
                rel_path = file
            summary.append(f"📁 {rel_path}")
            summary.append(f"   Lines: {analysis['lines']:,}")
            summary.append(f"   Functions: {analysis['functions']}")
            summary.append(f"   JSDoc Coverage: {analysis['jsdoc_coverage']:.1f}%")
            summary.append(f"   Complexity: {analysis['complexity_score']:.1f}")
            summary.append(f"   TypeScript Check: {'✅' if analysis['has_ts_check'] else '❌'}")
            summary.append("")
        
        summary.append("📈 Overall Statistics:")
        summary.append(f"   Total Files: {len(files)}")
        summary.append(f"   Total Lines: {total_lines:,}")
        summary.append(f"   Total Functions: {total_functions}")
        summary.append(f"   Files with JSDoc: {files_with_jsdoc}/{len(files)}")
        summary.append(f"   Files with @ts-check: {files_with_ts_check}/{len(files)}")
        summary.append("")
        
        summary.append("🎯 Recommended Enhancement Order:")
        summary.append("1. Enable @ts-check in deno.json (✅ DONE)")
        summary.append("2. Add @ts-check comments to individual files")
        summary.append("3. Enhance JSDoc annotations for complex functions")  
        summary.append("4. Add @typedef for complex data structures")
        summary.append("5. Consider runtime assertions for critical paths")
        
        return '\n'.join(summary)

def main():
    parser = argparse.ArgumentParser(description='Enhance JavaScript typing in FastLED WASM platform')
    parser.add_argument('--approach', choices=['jsdoc', 'ts-check', 'assertions', 'flow', 'summary'], 
                       default='summary', help='Enhancement approach to use')
    parser.add_argument('--file', type=Path, help='Specific file to enhance (optional)')
    parser.add_argument('--all', action='store_true', help='Apply to all JavaScript files')
    parser.add_argument('--dry-run', action='store_true', default=True, help='Show recommendations without making changes')
    parser.add_argument('--apply', action='store_true', help='Actually apply changes (overrides --dry-run)')
    
    args = parser.parse_args()
    
    enhancer = JSTypeEnhancer()
    
    if args.apply:
        args.dry_run = False
    
    if args.file:
        files_to_process = [args.file]
    elif args.all:
        files_to_process = enhancer.find_js_files()
    else:
        files_to_process = [None]  # For summary mode
    
    for file_path in files_to_process:
        if args.approach in enhancer.approaches:
            result = enhancer.approaches[args.approach](file_path, args.dry_run)
            
            if file_path:
                print(f"\n🔍 {args.approach.upper()} Enhancement for {file_path}:")
                print("-" * 60)
            
            print(result)
        else:
            print(f"❌ Unknown approach: {args.approach}")

if __name__ == '__main__':
    main() 
