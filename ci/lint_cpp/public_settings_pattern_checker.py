#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker to enforce the Public Settings Pattern for global configuration setters.

Every new public free function in `src/fl/**/*.h` whose name matches the regex
`^(set|enable|disable|use)_[a-z]` and that is declared at namespace scope
(inside `namespace fl {` at depth 1 — NOT inside a class, NOT in an anonymous
namespace, NOT in `fl::detail::` or any sub-namespace) MUST have a matching
`CFastLED::setX()` / `enableX()` / `disableX()` wrapper in `src/FastLED.h`
that delegates to it.

Why: `CFastLED` (the `FastLED` god instance) is the documented, discoverable
user API surface. Free-function-only global setters ratchet configuration knobs
into an undocumented second location that users won't find and that drifts out
of sync with the god instance.

Exemplar (`src/FastLED.h:1455`):
    // Free function — does the work
    namespace fl { void set_power_model(const PowerModelRGB& m) noexcept; }

    // CFastLED wrapper — what users call
    class CFastLED {
        inline void setPowerModel(const PowerModelRGB& model) {
            set_power_model(model);
        }
    };

Carve-outs (NOT flagged):
  - Functions in `fl::detail::`, `fl::isr::`, or any sub-namespace (depth > 1).
  - Functions inside anonymous `namespace { }` blocks.
  - Functions inside class/struct bodies (per-object setters, not global state).
  - Function pointer setters where the first parameter is a function-pointer type
    (e.g. `set_rgb_2_rgbw_function(func)`).
  - Per-object mutators: first parameter is a non-const pointer/reference to a
    user-defined (non-fundamental) type — heuristic for per-object config.
  - Lines in `// comments` or `/* ... */` blocks.
  - Lines with suppression `// nolint` or `// ok public_settings`.

Allowlist (grandfathered pre-rule offenders — do NOT flag):
  - `set_rgbw_colorimetric_profile`  (wrapped in PR #2715; allowlist is
    defence-in-depth for when the wrapper grep heuristic might miss it)
  - `set_input_gamut`                (allowlisted until a wrapper is shipped)

Scope: `src/fl/**/*.h` only (NOT third_party, NOT detail subdirs).
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# ---------------------------------------------------------------------------
# Grandfathered allowlist — names that predate the rule.
# These are NOT flagged regardless of wrapper presence.
# Remove an entry only after its CFastLED wrapper is merged and the grep
# heuristic has been verified to catch it cleanly.
# ---------------------------------------------------------------------------
GRANDFATHERED_NAMES: frozenset[str] = frozenset(
    [
        "set_rgbw_colorimetric_profile",  # Wrapped in PR #2715
        "set_input_gamut",  # Grandfathered; wrapper planned
        # Added in #2545 (colorimetric RGBW LUT). Wrappers planned.
        "enable_rgbw_colorimetric_lut",
        "disable_rgbw_colorimetric_lut",
        # Added in #2558 (RGBWW pipeline). Wrapper planned.
        "set_rgbww_colorimetric_profile",
    ]
)

# ---------------------------------------------------------------------------
# Additional carve-outs: function-pointer installer pattern.
# These names set a *callback*, not a global scalar/object.  The syntactic
# heuristic (first-param-is-non-fundamental) doesn't catch function pointers
# cleanly, so we name them explicitly.  Revisit if real violations slip through.
# ---------------------------------------------------------------------------
FUNCTION_POINTER_SETTERS: frozenset[str] = frozenset(
    [
        "set_rgb_2_rgbw_function",
        "set_rgb_2_rgbww_function",
    ]
)

# ---------------------------------------------------------------------------
# Pre-compiled regexes
# ---------------------------------------------------------------------------

# Declaration-like line: starts with a return type word, then a name that
# matches the pattern.  We require the function name to start the
# `(set|enable|disable|use)_[a-z]` pattern.
_SETTER_DECL = re.compile(
    r"^"
    r"(?:(?:inline|static|virtual|explicit|constexpr|FL_NOEXCEPT|FASTLED_FORCE_INLINE)\s+)*"  # optional qualifiers
    r"(?:[\w:]+(?:<[^>]*>)?(?:\s*[*&])?\s+)+"  # return type (e.g. "void ", "bool ", "int ")
    r"(?P<name>(set|enable|disable|use)_[a-z][a-zA-Z0-9_]*)"  # the setter name
    r"\s*\("  # open paren
)

# Matches "namespace <name>" — named namespace opening
_NAMED_NS = re.compile(r"^\s*namespace\s+(\w+)\s*\{")

# Matches anonymous namespace "namespace {" (no name)
_ANON_NS = re.compile(r"^\s*namespace\s*\{")

# Suppression on the same line
_SUPPRESSION = re.compile(r"//\s*(nolint|ok\s+public_settings)\b", re.IGNORECASE)

# Fundamental C++ types — used to distinguish per-object vs global setters.
# If the FIRST parameter starts with one of these, it's a scalar/global setter
# candidate.  If it's a user-defined type pointer/ref, it's per-object.
_FUNDAMENTAL_TYPES = frozenset(
    [
        "bool",
        "char",
        "short",
        "int",
        "long",
        "float",
        "double",
        "void",
        "unsigned",
        "signed",
        "u8",
        "u16",
        "u32",
        "u64",
        "s8",
        "s16",
        "s32",
        "s64",
        "uint8_t",
        "uint16_t",
        "uint32_t",
        "uint64_t",
        "int8_t",
        "int16_t",
        "int32_t",
        "int64_t",
        "size_t",
        "ssize_t",
        "ptrdiff_t",
    ]
)

# Matches first parameter extraction from a declaration line.
# Captures: full param list inside parens (simplified — single-line only).
_PARAM_LIST = re.compile(r"\(([^)]*)\)")


def _load_fastled_h() -> str:
    """Load the content of src/FastLED.h for cross-file wrapper checks."""
    fastled_h = PROJECT_ROOT / "src" / "FastLED.h"
    try:
        return fastled_h.read_text(encoding="utf-8")
    except OSError:
        return ""


def _is_wrapped_in_fastled_h(func_name: str, fastled_h_content: str) -> bool:
    """Return True if *func_name* appears inside the body of a CFastLED method
    in FastLED.h.

    Heuristic: we search for any non-comment, non-include line in FastLED.h
    that contains the function name as a bare word (not the #include line).
    This is intentionally permissive — false negatives (missing wrappers) are
    what we want to catch; false positives (spurious "found") are benign.
    """
    if not fastled_h_content:
        return False

    pattern = re.compile(r"\b" + re.escape(func_name) + r"\b")
    for line in fastled_h_content.splitlines():
        stripped = line.strip()
        # Skip the include line that brings in the header defining this function
        if stripped.startswith("#include"):
            continue
        # Skip comment-only lines
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        if pattern.search(line):
            return True
    return False


def _is_per_object_setter(decl_line: str) -> bool:
    """Return True if the declaration looks like a per-object mutator.

    Heuristic: if the first parameter is a non-const pointer (*) or non-const
    reference (&) to a non-fundamental type (i.e. a struct/class), the function
    is likely mutating an object the caller owns, not global state.

    Examples that return True (per-object — skip):
        void set_input_gamut(DiodeProfile* obj, InputGamut g)
        void set_color(CRGB& out, uint8_t r, uint8_t g, uint8_t b)

    Examples that return False (global state — flag):
        void set_power_model(const PowerModelRGB& model)
        void enable_rgbw_colorimetric_lut(int grid_n)
    """
    m = _PARAM_LIST.search(decl_line)
    if not m:
        return False
    params_raw = m.group(1).strip()
    if not params_raw:
        return False  # no params → global setter (e.g. disable_foo())

    # Split off the first parameter (stop at first comma that is not inside <>)
    depth = 0
    first_param: list[str] = []
    for ch in params_raw:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        elif ch == "," and depth == 0:
            break
        first_param.append(ch)
    first_param_str = "".join(first_param).strip()

    # Check for a non-const pointer or reference to user-defined type.
    # Pattern: optional "const", type tokens, then * or &, then optional name.
    # If "const" appears before the *, it is read-only — treat as global.
    # Function pointer parameters look like: `ret_type (*name)(args)` — skip too.
    if "(*" in first_param_str:
        return False  # function pointer — handled by FUNCTION_POINTER_SETTERS

    has_non_const_ptr_or_ref = False
    stripped_fp = first_param_str

    # Remove trailing parameter name (last word token)
    tokens = stripped_fp.split()
    # Check if last token is likely a name (no *, &, :, <)
    if tokens and re.match(r"^[a-zA-Z_]\w*$", tokens[-1]):
        tokens = tokens[:-1]
    param_type_str = " ".join(tokens)

    # If "const" is in the type tokens *before* the * or &, it is read-only
    is_const = "const" in param_type_str
    has_ptr_or_ref = "*" in param_type_str or "&" in param_type_str

    if has_ptr_or_ref and not is_const:
        # Check if the base type is fundamental
        # Strip *, &, and const to get the base type word
        base = re.sub(r"[*&\s]", " ", param_type_str)
        base = base.replace("const", "").strip()
        base_tokens = base.split()
        if base_tokens:
            base_type = base_tokens[0].lstrip(":").split("::")[-1]
            if base_type not in _FUNDAMENTAL_TYPES:
                has_non_const_ptr_or_ref = True

    return has_non_const_ptr_or_ref


class PublicSettingsPatternChecker(FileContentChecker):
    """Checker that flags fl::set_*/enable_*/disable_*/use_* free functions
    declared at namespace scope in src/fl/**/*.h without a CFastLED wrapper
    in src/FastLED.h.
    """

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}
        # Load FastLED.h content once at construction time.
        self._fastled_h_content: str = _load_fastled_h()

    def should_process_file(self, file_path: str) -> bool:
        normalized = file_path.replace("\\", "/")

        # Only header files
        if not normalized.endswith(".h"):
            return False

        # Only inside src/fl/ (absolute or relative paths)
        if "/src/fl/" not in normalized and not normalized.startswith("src/fl/"):
            return False

        # Exclude third_party
        if "/third_party/" in normalized:
            return False

        # Exclude .cpp.hpp pseudo-headers (they're implementation files)
        if normalized.endswith(".cpp.hpp"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Scan a src/fl/ header for unwrapped global setter declarations."""
        # Fast skip: if none of the trigger prefixes appear, bail immediately
        content = file_content.content
        if not any(kw in content for kw in ("set_", "enable_", "disable_", "use_")):
            return []

        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        # Scope stack: each entry is a string describing the scope kind:
        #   "fl"        — inside `namespace fl { ` (the target scope)
        #   "ns_sub"    — inside a named sub-namespace (fl::detail, fl::isr, …)
        #   "ns_anon"   — inside anonymous `namespace { `
        #   "class"     — inside a class/struct body
        #   "other"     — any other brace block (enum, function body, if, …)
        #
        # We push ONE entry per `{` and pop ONE per `}`.  This correctly
        # handles enums, inline functions, conditionals, etc.
        scope_stack: list[str] = []

        def _classify_open(code_line: str) -> str:
            """Classify what kind of scope a `{`-bearing line opens."""
            # Named namespace?
            ns_m = _NAMED_NS.match(code_line)
            if ns_m:
                name = ns_m.group(1)
                # Is the immediately enclosing scope "fl"?
                if scope_stack == ["fl"] or scope_stack == []:
                    # Top-level: "fl" itself or a first-level sub-namespace
                    if name == "fl":
                        return "fl"
                    # Anything else at top level is a non-fl namespace
                    return "ns_sub"
                else:
                    # Nested under something — always a sub-namespace
                    return "ns_sub"
            # Anonymous namespace?
            if _ANON_NS.match(code_line):
                return "ns_anon"
            # class/struct (not a forward declaration)?
            if re.search(r"\b(class|struct)\b", code_line):
                if not re.search(r"\b(class|struct)\b\s+\w+\s*;", code_line):
                    return "class"
            # Everything else (enum body, function body, if block, …)
            return "other"

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # ---- multi-line comment tracking ----
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue
            if in_multiline_comment:
                continue

            # Skip single-line comment lines
            if stripped.startswith("//"):
                continue

            # Strip inline comment for code analysis
            code = line.split("//")[0]

            opens = code.count("{")
            closes = code.count("}")

            # Push one scope entry per opening brace.
            # When multiple braces appear on one line (rare but valid, e.g.
            # `namespace fl { namespace detail {`), classify only the first
            # opening brace against the line and treat any extras as "other".
            if opens > 0:
                first_kind = _classify_open(code.strip())
                scope_stack.append(first_kind)
                for _ in range(opens - 1):
                    scope_stack.append("other")

            # Pop one scope entry per closing brace.
            for _ in range(closes):
                if scope_stack:
                    scope_stack.pop()

            # ---- check if we're at fl:: root scope (after brace updates) ----
            # The ONLY valid scope stack for a free function in `namespace fl`
            # is exactly `["fl"]`.  Any deeper nesting (class, sub-namespace,
            # anonymous namespace, enum body) disqualifies the line.
            if scope_stack != ["fl"]:
                continue

            # Skip preprocessor directives
            if stripped.startswith("#"):
                continue

            # ---- check for a setter declaration ----
            code_stripped = code.strip()
            if not code_stripped:
                continue

            # Fast pre-check before regex
            has_trigger = any(
                kw in code_stripped for kw in ("set_", "enable_", "disable_", "use_")
            )
            if not has_trigger:
                continue

            m = _SETTER_DECL.match(code_stripped)
            if not m:
                continue

            func_name = m.group("name")

            # Suppression comment on same line
            if _SUPPRESSION.search(line):
                continue

            # Allowlist: grandfathered offenders
            if func_name in GRANDFATHERED_NAMES:
                continue

            # Carve-out: function-pointer installers
            if func_name in FUNCTION_POINTER_SETTERS:
                continue

            # Carve-out: per-object setters (non-const ptr/ref first param)
            if _is_per_object_setter(code_stripped):
                continue

            # Cross-file check: is this function name used inside FastLED.h?
            if _is_wrapped_in_fastled_h(func_name, self._fastled_h_content):
                continue

            # Convert snake_case to camelCase for the suggested wrapper name
            parts = func_name.split("_")
            wrapper_name = parts[0] + "".join(p.capitalize() for p in parts[1:])

            violation_msg = (
                f"fl::{func_name}(...) declared here is a public global setter "
                f"but has no CFastLED::{wrapper_name}() wrapper in src/FastLED.h. "
                f"New global setters must follow the Public Settings Pattern — add "
                f"an `inline` one-liner on CFastLED that delegates to this free "
                f"function. See exemplar at src/FastLED.h:1455 "
                f"(setPowerModel -> fl::set_power_model) and the rule at "
                f"agents/docs/cpp-standards.md -> 'Public Settings Pattern'."
            )
            violations.append((line_number, violation_msg))

        if violations:
            self.violations[file_content.path] = violations

        return []  # Violations stored internally


def main() -> None:
    """Run public_settings_pattern checker standalone."""
    from ci.util.check_files import run_checker_standalone

    checker = PublicSettingsPatternChecker()
    run_checker_standalone(
        checker,
        [str(PROJECT_ROOT / "src" / "fl")],
        "Found fl:: global setters without CFastLED wrappers (Public Settings Pattern)",
        extensions=[".h"],
    )


if __name__ == "__main__":
    main()
