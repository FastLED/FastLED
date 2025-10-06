#!/usr/bin/env python3
"""
Backward compatibility wrapper for Docker build script.

This script has been moved to ci/docker/build_image.py
This wrapper is kept for backward compatibility.
"""

import sys
from ci.docker.build_image import main

if __name__ == "__main__":
    sys.exit(main())
