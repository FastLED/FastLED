# TASK2: Docker Infrastructure Consolidation & Refactoring

## Executive Summary

The FastLED project previously had **two separate Docker-related directories** (`ci/docker/` and `ci/dockerfiles/`) that served overlapping purposes. This document describes the consolidation that has been performed to merge them into a single directory structure.

---

## Current State Analysis

### 🔍 Problem: Duplicate Docker Directories

**ci/docker/** - Compilation Docker infrastructure
- `build_image.py` - Builds Docker images for PlatformIO compilation
- `prune_old_images.py` - Cleans up old Docker images
- `build.sh` - Shell script for building images
- `Dockerfile.base` - Base Dockerfile for compilation
- `Dockerfile.template` - Template for generating platform-specific Dockerfiles
- `README.md` - Documentation for Docker compilation
- `task.md` - Task tracking for Docker work
- `.dockerignore` - Docker ignore rules

**ci/docker/** - QEMU Docker infrastructure (consolidated from ci/dockerfiles/)
- `DockerManager.py` - Docker container management (used by QEMU)
- `qemu_esp32_docker.py` - QEMU runner using Docker
- `qemu_test_integration.py` - QEMU test integration
- `Dockerfile.qemu-esp32` - QEMU Dockerfile (UNUSED - we use espressif/idf)
- `Dockerfile.qemu-esp32-lite` - Lite QEMU Dockerfile (UNUSED)
- `docker-compose.yml` - Docker compose configuration (UNUSED)
- `README.md` - Documentation for QEMU Docker

### 📁 All Files Involved in --qemu Mode

#### Core Python Files
1. **test.py** (lines 80-227)
   - Entry point for `--qemu` flag
   - Calls DockerQEMURunner
   - Builds examples with `--merged-bin` flag
   - Manages QEMU test execution

2. **ci/docker/qemu_esp32_docker.py** (520 lines)
   - Main QEMU runner implementation
   - Docker container management for QEMU
   - Firmware preparation
   - QEMU command generation
   - Constants: `DEFAULT_DOCKER_IMAGE`, `QEMU_RISCV32_PATH`, `QEMU_XTENSA_PATH`, `QEMU_WRAPPER_SCRIPT_TEMPLATE`

3. **ci/docker/DockerManager.py** (13,308 bytes)
   - Generic Docker container management
   - Container lifecycle (create, start, stop, remove)
   - Volume mounting
   - Streaming output capture
   - Used by: qemu_esp32_docker.py

4. **ci/docker/qemu_test_integration.py** (5,877 bytes)
   - QEMU test integration helpers
   - Test result parsing
   - Output validation

5. **ci/ci-compile.py** (Docker mode sections)
   - `--docker` flag support (lines 133-338)
   - `--merged-bin` flag support
   - Docker compilation wrapper
   - Artifact copying from container to host
   - Uses: `ci.docker.build_image` module

6. **ci/build_docker_image_pio.py**
   - Builds PlatformIO Docker images
   - Uses: `ci.docker.build_image`

#### Supporting Files

7. **ci/docker/build_image.py** (5,203 bytes)
   - Docker image building utilities
   - Image tagging and versioning
   - Used by: ci-compile.py, build_docker_image_pio.py

8. **ci/docker/prune_old_images.py** (10,495 bytes)
   - Docker image cleanup
   - Removes stale images to save disk space

#### Dockerfiles (UNUSED for QEMU) - ✅ REMOVED

9. **~~ci/docker/Dockerfile.qemu-esp32~~** - ✅ DELETED
   - Was unused - we use `espressif/idf:latest` instead

10. **~~ci/docker/Dockerfile.qemu-esp32-lite~~** - ✅ DELETED
    - Was unused - early prototype

11. **~~ci/docker/docker-compose.yml~~** - ✅ DELETED
    - Was unused - we use direct Docker API calls via DockerManager

#### Dockerfiles (USED for Compilation)

12. **ci/docker/Dockerfile.base**
    - Base image for PlatformIO compilation
    - Used by build_image.py

13. **ci/docker/Dockerfile.template**
    - Template for platform-specific images
    - Used by build_image.py

#### GitHub Actions Workflows

14. **.github/workflows/qemu_tobozo2.yml**
    - Working QEMU workflow using tobozo/esp32-qemu-sim
    - Tests ESP32, ESP32-S3, ESP32-C3
    - Uses merged-bin approach

15. **.github/workflows/qemu_template.yml**
    - Template for QEMU workflows
    - 33,485 bytes

16. **.github/workflows/qemu_template2.yml**
    - Second template variant
    - 10,573 bytes

17. **.github/workflows/qemu_esp32dev_test.yml**
18. **.github/workflows/qemu_esp32c3_test.yml**
19. **.github/workflows/qemu_esp32s3_test.yml**
20. **.github/workflows/qemu_esp32p4_test.yml**
21. **.github/workflows/qemu_esp32dev_tobozo2_test.yml**

#### Documentation

22. **ci/docker/README.md**
    - Documentation for Docker compilation
    - Duplicates some content from ci/docker/README.qemu.md

23. **ci/docker/README.qemu.md**
    - Documentation for QEMU Docker
    - Duplicates some content from ci/docker/README.md

24. **ci/docker/task.md**
    - Task tracking for Docker infrastructure

---

## Problems with Current Structure

### 🔴 Critical Issues

1. **Confusing Directory Names**
   - ~~`ci/docker/` vs `ci/dockerfiles/` - which is which?~~ ✅ RESOLVED
   - No clear separation of concerns
   - New developers don't know where to look

2. **Module Import Confusion**
   ```python
   from ci.docker.build_image import ...        # Compilation
   from ci.docker.DockerManager import ...      # QEMU
   ```
   - Two different docker-related imports
   - No logical organization

3. **Duplicate Functionality**
   - Both directories contain Docker management code
   - Both have README.md files
   - Both have Dockerfiles (some unused)

4. **Unused Files** - ✅ CLEANED UP
   - ~~`ci/docker/Dockerfile.qemu-esp32`~~ - ✅ DELETED (we use espressif/idf)
   - ~~`ci/docker/Dockerfile.qemu-esp32-lite`~~ - ✅ DELETED
   - ~~`ci/docker/docker-compose.yml`~~ - ✅ DELETED

5. **Cross-Directory Dependencies**
   - `ci-compile.py` imports from `ci.docker.build_image`
   - `test.py` imports from `ci.docker.qemu_esp32_docker`
   - No clear dependency hierarchy

### 🟡 Maintenance Issues

6. **Documentation Fragmentation**
   - Docker knowledge split across two READMEs
   - Task tracking in multiple places
   - Hard to find information

7. **Testing Fragmentation**
   - QEMU tests in test.py
   - Docker compilation tests elsewhere
   - No unified testing strategy

---

## Proposed Solution: Consolidate into `ci/docker/`

### 📂 New Directory Structure

```
ci/
└── docker/
    ├── __init__.py
    ├── README.md                    # Unified documentation
    ├── .dockerignore
    │
    ├── compilation/                 # Compilation-related Docker
    │   ├── __init__.py
    │   ├── build_image.py          # Image building
    │   ├── prune_old_images.py     # Cleanup utilities
    │   ├── build.sh                # Build script
    │   ├── Dockerfile.base         # Base image
    │   └── Dockerfile.template     # Template for platforms
    │
    ├── qemu/                        # QEMU-related Docker
    │   ├── __init__.py
    │   ├── runner.py               # qemu_esp32_docker.py → runner.py
    │   ├── test_integration.py     # qemu_test_integration.py
    │   └── constants.py            # QEMU constants (paths, images)
    │
    ├── common/                      # Shared Docker utilities
    │   ├── __init__.py
    │   ├── manager.py              # DockerManager.py
    │   └── utils.py                # Common utilities
    │
    └── docs/
        ├── compilation.md          # Compilation Docker docs
        ├── qemu.md                 # QEMU Docker docs
        └── REFACTOR.md             # This document
```

### 🔄 Import Changes

**Before:**
```python
from ci.docker.build_image import DockerImageBuilder
from ci.docker.DockerManager import DockerManager
from ci.docker.qemu_esp32_docker import DockerQEMURunner
```

**After:**
```python
from ci.docker.compilation.build_image import DockerImageBuilder
from ci.docker.common.manager import DockerManager
from ci.docker.qemu.runner import DockerQEMURunner
```

### 📝 File Renames

| Old Path | New Path | Reason |
|----------|----------|--------|
| `ci/docker/qemu_esp32_docker.py` | `ci/docker/qemu/runner.py` | Clearer naming |
| `ci/docker/DockerManager.py` | `ci/docker/common/manager.py` | Shared utility |
| `ci/docker/qemu_test_integration.py` | `ci/docker/qemu/test_integration.py` | Better organization |
| `ci/docker/build_image.py` | `ci/docker/compilation/build_image.py` | Better organization |
| `ci/docker/prune_old_images.py` | `ci/docker/compilation/prune_old_images.py` | Better organization |

---

## Migration Plan

### Phase 1: Preparation ✅
- [x] Document current state (this file)
- [ ] Create `ci/docker/common/` directory
- [ ] Create `ci/docker/qemu/` directory
- [ ] Create `ci/docker/compilation/` directory

### Phase 2: Move Shared Components
- [ ] Move `DockerManager.py` → `ci/docker/common/manager.py`
- [ ] Update all imports in:
  - `ci/docker/qemu_esp32_docker.py`
  - `ci/docker/qemu_test_integration.py`
  - Any other files that import DockerManager

### Phase 3: Move QEMU Components
- [ ] Move `qemu_esp32_docker.py` → `ci/docker/qemu/runner.py`
- [ ] Move `qemu_test_integration.py` → `ci/docker/qemu/test_integration.py`
- [ ] Extract constants to `ci/docker/qemu/constants.py`
- [ ] Update imports in:
  - `test.py`
  - Any other QEMU-related files

### Phase 4: Move Compilation Components
- [ ] Move `build_image.py` → `ci/docker/compilation/build_image.py`
- [ ] Move `prune_old_images.py` → `ci/docker/compilation/prune_old_images.py`
- [ ] Move `build.sh` → `ci/docker/compilation/build.sh`
- [ ] Move Dockerfiles → `ci/docker/compilation/`
- [ ] Update imports in:
  - `ci/ci-compile.py`
  - `ci/build_docker_image_pio.py`

### Phase 5: Archive Unused Files ✅ COMPLETED
- [x] ~~Move unused Dockerfiles to `ci/docker/archive/`~~ - **DELETED INSTEAD**
  - ✅ Deleted `Dockerfile.qemu-esp32` (unused, we use espressif/idf:latest)
  - ✅ Deleted `Dockerfile.qemu-esp32-lite` (unused prototype)
  - ✅ Deleted `docker-compose.yml` (unused, we use DockerManager direct API)
- [x] Git history preserves these files for reference if needed

### Phase 6: Documentation
- [ ] Merge README files into unified `ci/docker/README.md`
- [ ] Create `ci/docker/docs/compilation.md`
- [ ] Create `ci/docker/docs/qemu.md`
- [ ] Update `CLAUDE.md` with new paths
- [ ] Update `ci/AGENTS.md` with new structure

### Phase 7: Testing & Validation
- [ ] Run `bash lint` - ensure all imports work
- [ ] Run `bash compile esp32c3 Blink` - test Docker compilation
- [ ] Run `uv run test.py --qemu esp32c3 BlinkParallel` - test QEMU
- [ ] Run full test suite
- [ ] Validate GitHub Actions still work

### Phase 8: Cleanup
- [x] Remove old `ci/dockerfiles/` directory
- [ ] Remove old files from `ci/docker/` root
- [ ] Update `.gitignore` if needed
- [ ] Clean up stale imports

---

## Benefits of Consolidation

### 🎯 Immediate Benefits

1. **Clear Organization**
   - `compilation/` - everything for building with Docker
   - `qemu/` - everything for QEMU testing
   - `common/` - shared utilities

2. **Better Discoverability**
   - Developers know exactly where to look
   - Logical grouping by functionality
   - Consistent naming conventions

3. **Reduced Confusion**
   - Only ONE docker directory
   - Clear import paths
   - No more "which docker directory?"

4. **Easier Maintenance**
   - Changes to Docker logic in one place
   - Shared utilities in `common/`
   - Documentation in one location

### 🚀 Long-term Benefits

5. **Extensibility**
   - Easy to add new Docker-based tools
   - Clear pattern to follow
   - Room for growth

6. **Testing Improvements**
   - Unified testing strategy
   - Shared test utilities
   - Better CI/CD integration

7. **Documentation Quality**
   - Single source of truth
   - Easier to keep updated
   - Better for new contributors

---

## Risk Analysis

### 🟢 Low Risk

- Moving files (Git preserves history)
- Updating imports (caught by linting)
- Documentation consolidation

### 🟡 Medium Risk

- Breaking GitHub Actions workflows (test thoroughly)
- Third-party tools that import these modules
- Cached Docker images with old paths

### 🔴 Mitigation Strategies

1. **Incremental Migration**
   - Move one subsystem at a time
   - Test after each phase
   - Keep old imports working temporarily with deprecation warnings

2. **Backwards Compatibility**
   - Create temporary shim modules in old locations:
   ```python
   # ci/docker/DockerManager.py (now in ci/docker/)
   import warnings
   from ci.docker.common.manager import DockerManager

   warnings.warn(
       "ci.docker.DockerManager will be moved to ci.docker.common.manager in a future release. "
       "Please update your imports.",
       DeprecationWarning,
       stacklevel=2
   )
   ```

3. **Comprehensive Testing**
   - Run all tests before/after migration
   - Test both local and GitHub Actions
   - Validate Docker image building and QEMU execution

---

## Success Criteria

### ✅ Migration Complete When:

1. [ ] All files moved to new structure
2. [ ] All imports updated and working
3. [ ] All tests passing (C++, Python, examples)
4. [ ] Docker compilation working: `bash compile esp32c3 Blink`
5. [ ] QEMU testing working: `uv run test.py --qemu esp32c3 BlinkParallel`
6. [ ] GitHub Actions workflows passing
7. [ ] Documentation updated and accurate
8. [ ] `bash lint` passes
9. [x] Old `ci/dockerfiles/` directory removed
10. [ ] No deprecation warnings in normal usage

### 📊 Quality Metrics

- **Import clarity**: Single logical path for each component
- **Documentation**: One comprehensive README + specialized docs
- **Discoverability**: New developers find files within 30 seconds
- **Maintainability**: Changes to Docker code only touch 1-2 files max

---

## Open Questions

1. **Should we keep backwards-compatible shims?**
   - Pro: Doesn't break external tools
   - Con: Technical debt
   - **Recommendation**: Yes, with deprecation warnings for 1-2 releases

2. **Should we rename DockerManager to something more specific?**
   - Current: `DockerManager`
   - Alternative: `ContainerManager`, `DockerRunner`, `DockerClient`
   - **Recommendation**: Keep as `DockerManager` for now (clear enough)

3. **Should compilation and QEMU share container management?**
   - Currently: Both use similar Docker patterns
   - Opportunity: Extract common patterns to `common/manager.py`
   - **Recommendation**: Yes, consolidate shared logic

4. **~~What to do with archive files?~~** ✅ RESOLVED
   - **Decision**: Deleted unused files (docker-compose.yml, Dockerfile.qemu-esp32*)
   - Git history preserves them if ever needed for reference

---

## Timeline Estimate

**Estimated effort**: 4-6 hours for one developer

- Phase 1-2: 30 minutes (setup + move shared components)
- Phase 3: 1 hour (move QEMU components + update imports)
- Phase 4: 1 hour (move compilation components + update imports)
- Phase 5: 15 minutes (archive unused files)
- Phase 6: 1 hour (documentation consolidation)
- Phase 7: 1-2 hours (comprehensive testing)
- Phase 8: 30 minutes (cleanup)
- Buffer: 30 minutes (unexpected issues)

---

## Related Documents

- **TASK.md** - Docker-based QEMU testing implementation (completed)
- **ci/docker/README.md** - Current Docker compilation documentation
- **ci/docker/README.qemu.md** - Current QEMU Docker documentation
- **CLAUDE.md** - Project-wide AI agent guidelines
- **ci/AGENTS.md** - CI/build-specific AI agent guidelines

---

## Appendix: Current Import Graph

```
test.py
  └── ci.docker.qemu_esp32_docker.DockerQEMURunner
        └── ci.docker.DockerManager.DockerManager

ci/ci-compile.py (Docker mode)
  └── ci.docker.build_image.DockerImageBuilder

ci/build_docker_image_pio.py
  └── ci.docker.build_image.DockerImageBuilder

ci/docker/qemu_test_integration.py
  └── ci.docker.DockerManager.DockerManager
```

## Appendix: Proposed Import Graph

```
test.py
  └── ci.docker.qemu.runner.DockerQEMURunner
        └── ci.docker.common.manager.DockerManager

ci/ci-compile.py (Docker mode)
  └── ci.docker.compilation.build_image.DockerImageBuilder
        └── ci.docker.common.manager.DockerManager (optional)

ci/build_docker_image_pio.py
  └── ci.docker.compilation.build_image.DockerImageBuilder

ci/docker/qemu/test_integration.py
  └── ci.docker.common.manager.DockerManager
```

Notice: **All Docker management flows through `ci.docker.common.manager`**

---

## Conclusion

The initial consolidation of `ci/dockerfiles/` into `ci/docker/` has been completed. Further refactoring into subdirectories (`compilation/`, `qemu/`, `common/`) will:

- ✅ Eliminate confusion about which directory to use
- ✅ Provide clear organization by functionality
- ✅ Reduce duplicate documentation and code
- ✅ Make the codebase more maintainable
- ✅ Improve developer onboarding experience

**Recommendation**: Proceed with migration in phases, with comprehensive testing after each phase.
