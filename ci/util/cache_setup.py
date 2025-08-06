#!/usr/bin/env python3
"""
Standalone cache setup script for PlatformIO builds with Python fake compilers.
This script is executed by PlatformIO as a post-build extra_script.

Configuration is passed through environment variables to avoid template string issues.
"""

# Import env and try to import projenv
Import("env")
import os
import shutil
import json
import sys
from pathlib import Path

# Try to import projenv if it exists
try:
    Import("projenv")
    has_projenv = True
except:
    has_projenv = False
    projenv = None

# Add project root to Python path for fake compiler imports
# Note: We're in the build directory, go up to project root
current_dir = Path(os.getcwd()).resolve()
project_root = current_dir.parent.parent  # Go up from .build/pio/board to project root
if str(project_root) not in sys.path:
    sys.path.insert(0, str(project_root))

try:
    from ci.util.fake_compiler import create_fake_toolchain, get_platform_packages_paths
    fake_compiler_available = True
except ImportError as e:
    print("WARNING: Could not import fake compiler module: " + str(e))
    fake_compiler_available = False

# Debug: Dump the environment state to disk for inspection
env_dump = {}
for key in env.Dictionary():
    try:
        value = env[key]
        # Convert to string to avoid JSON serialization issues
        env_dump[key] = str(value)
    except:
        env_dump[key] = "<error getting value>"

# Write environment dump to disk
env_dump_path = "env_dump.json"
with open(env_dump_path, "w") as f:
    json.dump(env_dump, f, indent=2)
print("Environment state dumped to: " + env_dump_path)

# Also dump projenv if available
if has_projenv:
    projenv_dump = {}
    for key in projenv.Dictionary():
        try:
            value = projenv[key]
            projenv_dump[key] = str(value)
        except:
            projenv_dump[key] = "<error getting value>"
    
    projenv_dump_path = "projenv_dump.json"
    with open(projenv_dump_path, "w") as f:
        json.dump(projenv_dump, f, indent=2)
    print("Projenv state dumped to: " + projenv_dump_path)

# Read cache configuration from environment variables
cache_type = os.environ.get("FASTLED_CACHE_TYPE", "sccache")
cache_executable = os.environ.get("FASTLED_CACHE_EXECUTABLE", "")
sccache_path = os.environ.get("FASTLED_SCCACHE_PATH", "")
sccache_dir = os.environ.get("FASTLED_SCCACHE_DIR", "")
sccache_cache_size = os.environ.get("FASTLED_SCCACHE_CACHE_SIZE", "2G")
xcache_path = os.environ.get("FASTLED_XCACHE_PATH", "")
debug_enabled = os.environ.get("FASTLED_CACHE_DEBUG", "0") == "1"

print("Cache configuration from environment:")
print("  Cache type: " + cache_type)
print("  Cache executable: " + cache_executable)
print("  SCCACHE path: " + sccache_path)
print("  SCCACHE dir: " + sccache_dir)
print("  Debug enabled: " + str(debug_enabled))

# Set up cache environment variables for subprocess execution
if sccache_dir:
    env.Append(ENV={"SCCACHE_DIR": sccache_dir})
    os.environ["SCCACHE_DIR"] = sccache_dir

if sccache_cache_size:
    env.Append(ENV={"SCCACHE_CACHE_SIZE": sccache_cache_size})
    os.environ["SCCACHE_CACHE_SIZE"] = sccache_cache_size

if debug_enabled:
    env.Append(ENV={"XCACHE_DEBUG": "1", "SCCACHE_DEBUG": "1"})
    if has_projenv:
        projenv.Append(ENV={"XCACHE_DEBUG": "1", "SCCACHE_DEBUG": "1"})

# Check if cache is available and fake compiler system can be used
USE_CACHE = False

if fake_compiler_available and cache_executable:
    if cache_type == "xcache":
        # For xcache, check if the Python script exists and sccache is available
        USE_CACHE = (cache_executable and xcache_path and 
                     Path(xcache_path).exists() and sccache_path and shutil.which(sccache_path))
        
        if USE_CACHE:
            print("xcache wrapper detected and configured for Python fake compilers")
            print("  xcache path: " + str(xcache_path))
            print("  cache executable: " + str(cache_executable))
    else:
        # For sccache/ccache, check if executable is in PATH
        USE_CACHE = shutil.which(cache_executable) is not None
        
        if USE_CACHE:
            print(str(cache_type) + " detected and configured for Python fake compilers")
            print("  cache executable: " + str(cache_executable))
elif not fake_compiler_available:
    print("WARNING: Python fake compiler system not available, cache will be disabled")
else:
    print("Cache executable not found: " + str(cache_executable) + ", cache will be disabled")

if USE_CACHE:
    # Get current compilers from environment
    original_cc = env.get("CC")
    original_cxx = env.get("CXX")
    
    print("DEBUG: Found compilers in env:")
    print("  CC: " + str(original_cc) + " (type: " + str(type(original_cc)) + ")")
    print("  CXX: " + str(original_cxx) + " (type: " + str(type(original_cxx)) + ")")
    
    # Extract compiler information for fake compiler generation
    def extract_compiler_info(compiler_env_var):
        """Extract compiler name from environment variable value."""
        if not compiler_env_var:
            return None
        
        if isinstance(compiler_env_var, list):
            return str(compiler_env_var[0]) if compiler_env_var else None
        else:
            # Handle string values like "arm-none-eabi-gcc" or "gcc"
            return str(compiler_env_var).split()[0]
    
    cc_name = extract_compiler_info(original_cc) or "gcc"
    cxx_name = extract_compiler_info(original_cxx) or "g++"
    
    print("Extracted compiler names:")
    print("  CC name: " + str(cc_name))
    print("  CXX name: " + str(cxx_name))
    
    # Get platform packages paths for toolchain resolution
    platform_packages = get_platform_packages_paths()
    print("Found " + str(len(platform_packages)) + " platform package directories")
    
    # Create toolchain info for fake compiler generation
    toolchain_info = {
        "CC": cc_name,
        "CXX": cxx_name,
    }
    
    # Create cache config for fake compiler system
    cache_config = {
        "CACHE_TYPE": cache_type,
        "CACHE_EXECUTABLE": cache_executable,
        "SCCACHE_PATH": sccache_path,
        "SCCACHE_DIR": sccache_dir,
        "XCACHE_PATH": xcache_path,
    }
    
    # Create fake compiler scripts using Python system
    fake_compilers_dir = Path(current_dir) / "fake_compilers"
    fake_tools = create_fake_toolchain(
        toolchain_info=toolchain_info,
        cache_config=cache_config,
        platform_packages_paths=platform_packages,
        output_dir=fake_compilers_dir,
        debug=debug_enabled
    )
    
    if fake_tools:
        # Use Python fake compilers instead of batch scripts
        new_cc = fake_tools.get("CC")
        new_cxx = fake_tools.get("CXX")
        
        if new_cc and new_cxx:
            print("Created Python fake compilers:")
            print("  CC: " + str(new_cc))
            print("  CXX: " + str(new_cxx))
            
            # Apply to both environments
            env.Replace(CC=new_cc, CXX=new_cxx)
            if has_projenv:
                projenv.Replace(CC=new_cc, CXX=new_cxx)
                print("Applied Python fake compilers to both env and projenv")
            else:
                print("Applied Python fake compilers to env (projenv not available)")
            
            # Apply to library builders (critical for framework caching)
            try:
                for lib_builder in env.GetLibBuilders():
                    lib_builder.env.Replace(CC=new_cc, CXX=new_cxx)
                    print("Applied Python fake compilers to library builder: " + str(getattr(lib_builder, 'name', 'unnamed')))
            except Exception as e:
                print("WARNING: Could not apply to library builders: " + str(e))
            
            print("Python fake compiler cache enabled: " + str(cache_type))
            print("  Original CC: " + str(original_cc))
            print("  Original CXX: " + str(original_cxx))
            print("  Fake CC: " + str(new_cc))
            print("  Fake CXX: " + str(new_cxx))
        else:
            print("ERROR: Failed to create Python fake compilers, falling back to no cache")
            USE_CACHE = False
    else:
        print("ERROR: Python fake compiler creation failed, falling back to no cache")
        USE_CACHE = False

if not USE_CACHE:
    if cache_executable:
        print("Warning: " + str(cache_type) + " setup failed; using default compilers")
    else:
        print("No cache executable configured; using default compilers")

print("Python fake compiler cache environment configured successfully")
