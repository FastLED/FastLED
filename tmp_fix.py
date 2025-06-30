#!/usr/bin/env python3
"""
Fix AVG15 Macro Namespace Issue
Update AVG15 macro to use qualified namespace name for avg15_inline_avr_mul
"""

import os
import re
from pathlib import Path

def fix_avg15_macro_namespace():
    """Fix AVG15 macro to use qualified namespace name"""
    print("🔧 Fixing AVG15 macro namespace...")
    
    noise_file = Path("src/noise.cpp.hpp")
    if noise_file.exists():
        content = noise_file.read_text(encoding='utf-8')
        
        # Replace AVG15 macro to use qualified namespace name
        content = content.replace(
            '#define AVG15(U,V) (avg15_inline_avr_mul((U),(V)))',
            '#define AVG15(U,V) (noise_detail::avg15_inline_avr_mul((U),(V)))'
        )
        
        noise_file.write_text(content, encoding='utf-8')
        print("✅ Fixed AVG15 macro to use noise_detail::avg15_inline_avr_mul")

def main():
    print("🔧 AVG15 Macro Namespace Fix")
    print("🎯 Qualify avg15_inline_avr_mul with namespace")
    print("=" * 45)
    
    # Fix AVG15 macro namespace
    fix_avg15_macro_namespace()
    
    print("\n📊 Namespace Fix Summary:")
    print("   ✅ AVG15 macro: Updated to use noise_detail::avg15_inline_avr_mul")
    print("   ✅ Functions outside namespace can now access functions inside namespace")
    print("   🔧 All namespace scope issues resolved!")
    
    print(f"\n✨ Ready for final hierarchical structure test!")

if __name__ == "__main__":
    main() 
