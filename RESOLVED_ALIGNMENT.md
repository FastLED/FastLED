# Plan to Resolve Alignment Issues

## Problem Statement
The current codebase exhibits ad-hoc alignment implementations, particularly in `FixedVector`, where containers force an alignment rather than deriving it from their contained data. This leads to incorrect alignment and potential issues, especially with `fl::function` on Emscripten, which has specific function pointer alignment requirements.

## Goal
To refactor alignment handling to be correct, idiomatic, and data-driven, eliminating ad-hoc solutions and properly addressing `fl::function`'s unique needs.

## Plan of Action

### Phase 1: Investigation and Understanding

1.  **Analyze `FixedVector` and other relevant containers:**
    *   Identify all instances of ad-hoc alignment (e.g., manual padding, `__attribute__((aligned))`, or similar non-standard approaches) within `FixedVector` and any other data structures that hold elements requiring specific alignment.
    *   Understand how the current alignment is being determined and applied.
    *   Examine how `FixedVector`'s internal buffer or storage is allocated and aligned.
2.  **Deep Dive into `fl::function` Alignment:**
    *   Investigate the existing implementation of `fl::function` to understand how it currently handles alignment.
    *   Research Emscripten's specific requirements for function pointer alignment to confirm the exact constraints.
    *   Determine if `fl::function`'s alignment is currently being correctly propagated or if it's being overridden by container alignment.
3.  **Identify Core Alignment Principles:**
    *   Review C++ standard features for alignment (`alignof`, `alignas`, `std::aligned_storage`, `std::max_align_t`).
    *   Determine the most appropriate and idiomatic way to declare and enforce alignment for types and containers.

### Phase 2: Refactoring and Implementation

1.  **Define a Project-Wide Alignment Strategy:**
    *   Establish a clear convention for how containers should determine their alignment based on the `alignof` their contained type.
    *   Prioritize using standard C++ features (`alignof`, `alignas`) over compiler-specific extensions where possible.
2.  **Refactor `FixedVector` Alignment:**
    *   Remove all ad-hoc alignment logic from `FixedVector`.
    *   Modify `FixedVector`'s internal storage to correctly align itself to `alignof(T)` where `T` is the element type. This might involve using `std::aligned_storage` or ensuring the underlying memory allocation respects the required alignment.
    *   Ensure that the container's alignment is always at least as strict as the strictest alignment requirement of its elements.
3.  **Address `fl::function` Specifics:**
    *   If `fl::function` requires a stricter alignment than its natural `alignof` (e.g., due to Emscripten's function pointer requirements), ensure this is explicitly handled.
    *   This might involve `alignas` on the `fl::function` type itself, or a specialized `aligned_storage` for `fl::function` within containers. The goal is to make this explicit and correct, not ad-hoc.
4.  **Review and Adjust Other Containers/Types:**
    *   Based on the findings from Phase 1, apply the new alignment strategy to any other custom containers or types that might be affected by or contribute to alignment issues.

### Phase 3: Verification and Testing

1.  **Develop/Enhance Unit Tests:**
    *   Create specific unit tests to verify the alignment of `FixedVector` instances with various data types, including those with strict alignment requirements.
    *   Add tests to confirm `fl::function` instances (especially within containers) are correctly aligned according to Emscripten's requirements.
    *   Use `reinterpret_cast<uintptr_t>(ptr) % alignment == 0` checks where appropriate to assert correct alignment.
2.  **Cross-Platform Testing:**
    *   Crucially, test the changes on Emscripten to ensure the `fl::function` alignment is correctly handled in that environment.
    *   Verify behavior on other target platforms (e.g., native builds) to ensure no regressions.
3.  **Performance Considerations:**
    *   Monitor performance after changes to ensure that correct alignment does not introduce unexpected overhead. (Correct alignment often improves performance, but it's worth verifying).
4.  **Code Review:**
    *   Seek a thorough code review to ensure the new alignment strategy is correctly implemented and adheres to best practices.
