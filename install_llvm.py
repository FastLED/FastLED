#!/usr/bin/env python3

import os
import sys
import tempfile
import tarfile
import urllib.request
from pathlib import Path


def main() -> None:
    """Install LLVM toolchain using llvm-installer package."""
    try:
        # Import llvm-installer
        from llvm_installer import LlvmInstaller
        import sys_detection
    except ImportError as e:
        print(f"Error: Missing required packages. Please install with: uv add llvm-installer")
        print(f"Import error: {e}")
        sys.exit(1)
    
    # Get target directory from command line
    if len(sys.argv) != 2:
        print("Usage: python install_llvm.py <target_directory>")
        sys.exit(1)
    
    target_dir = Path(sys.argv[1])
    target_dir.mkdir(parents=True, exist_ok=True)
    
    print("Detecting system configuration...")
    try:
        local_sys_conf = sys_detection.local_sys_conf()
        detected_os = local_sys_conf.short_os_name_and_version()
        architecture = local_sys_conf.architecture
        print(f"Detected OS: {detected_os}")
        print(f"Architecture: {architecture}")
        
        # Use Ubuntu 24.04 as fallback since it's known to work
        # This package seems to be from YugabyteDB and has limited OS support
        working_os = "ubuntu24.04"
        if detected_os.startswith('ubuntu24') or detected_os.startswith('ubuntu22'):
            working_os = detected_os
        
        print(f"Using OS variant: {working_os}")
        
        llvm_installer = LlvmInstaller(
            short_os_name_and_version=working_os,
            architecture=architecture
        )
        
        # Try LLVM 18 (latest stable)
        version = 18
        print(f"Installing LLVM version {version}...")
        llvm_url = llvm_installer.get_llvm_url(major_llvm_version=version)
        print(f"Found LLVM {version} at: {llvm_url}")
        
        # Download and extract
        print("Downloading LLVM package...")
        with tempfile.NamedTemporaryFile(delete=False, suffix='.tar.gz') as tmp_file:
            urllib.request.urlretrieve(llvm_url, tmp_file.name)
            
            print("Extracting LLVM package...")
            with tarfile.open(tmp_file.name, 'r:gz') as tar:
                tar.extractall(target_dir, filter='data')
        
        os.unlink(tmp_file.name)
        
        # Find the extracted directory and verify clang exists
        for extracted_dir in target_dir.iterdir():
            if extracted_dir.is_dir():
                clang_path = extracted_dir / "bin" / "clang"
                
                if clang_path.exists():
                    print(f"✅ Successfully installed LLVM {version}")
                    print(f"   Installation directory: {extracted_dir}")
                    print(f"   Clang binary: {clang_path}")
                    
                    # List available tools
                    bin_dir = extracted_dir / "bin"
                    if bin_dir.exists():
                        tools = [f.name for f in bin_dir.iterdir() if f.is_file() and not f.name.endswith('.dll')]
                        print(f"   Available tools: {', '.join(sorted(tools[:10]))}{'...' if len(tools) > 10 else ''}")
                    
                    return
        
        print("❌ LLVM downloaded but clang not found in expected location")
        sys.exit(1)
        
    except Exception as e:
        print(f"Error during installation: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()