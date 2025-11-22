#!/usr/bin/env bash
#
# Lightweight static checks for kernel C style rules.
# Intended for use from the Makefile and as a pre-commit hook.
#
# This is not a full verifier; it catches obvious violations of
# docs/KERNEL_C_STYLE.md so problems are found early.
#

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

EXIT=0

echo "[check-kernel-c-style] Running style checks..."

# Common file set: all .c/.h under core, arch, drivers, mm, include
find_sources() {
  find core arch drivers mm include -type f \( -name '*.c' -o -name '*.h' \)
}

warn() {
  echo "  [FAIL] $1"
  EXIT=1
}

check_forbidden_patterns() {
  local desc="$1"
  shift
  local pattern="$1"
  shift || true

  # shellcheck disable=SC2046
  local matches
  matches=$(grep -REn --color=never "$pattern" "$@" 2>/dev/null || true)
  if [[ -n "$matches" ]]; then
    # Filter out lines where the matched token is only in a comment:
    # lines whose content (after "file:line:") starts with //, /*, or *.
    matches=$(printf "%s\n" "$matches" | \
      grep -Ev '^[^:]+:[0-9]+:[[:space:]]*//|^[^:]+:[0-9]+:[[:space:]]*/\*|^[^:]+:[0-9]+:[[:space:]]*\*' || true)
  fi

  if [[ -n "$matches" ]]; then
    warn "$desc"
    echo "$matches"
  fi
}

SRC_FILES=($(find_sources))

# 1) Forbidden libc-style functions in kernel code (outside lib/)
#    We allow these to appear in docs and lib/, but not in core/arch/mm/drivers/include.

check_forbidden_patterns "Forbidden libc allocation functions (malloc/calloc/realloc/free) in kernel code" \
  '\b(malloc|calloc|realloc|free)\s*\(' "${SRC_FILES[@]}"

check_forbidden_patterns "Forbidden libc string/printf functions (printf/sprintf/strcpy/strcat/strlen) in kernel code" \
  '\b(printf|sprintf|vsprintf|strcpy|strcat|strlen)\s*\(' "${SRC_FILES[@]}"

# 2) setjmp/longjmp
check_forbidden_patterns "Forbidden setjmp/longjmp usage in kernel code" \
  '\b(setjmp|longjmp)\s*\(' "${SRC_FILES[@]}"

# 3) alloca / VLAs (we can reliably detect alloca, VLAs are heuristic and not enforced here)
check_forbidden_patterns "Forbidden alloca() usage in kernel code" \
  '\balloca\s*\(' "${SRC_FILES[@]}"

# 4) Floating point types in kernel code
check_forbidden_patterns "Floating point types are not allowed in kernel code (float/double/long double)" \
  '\b(float|double|long double)\b' "${SRC_FILES[@]}"

# 5) Inline assembly outside arch/
NON_ARCH_FILES=($(find core drivers mm include -type f \( -name '*.c' -o -name '*.h' \)))
if [[ ${#NON_ARCH_FILES[@]} -gt 0 ]]; then
  # Allow inline asm in special cases:
  # - barrier() macro in include/kernel/types.h
  # - assertions in include/kernel/assert.h (need cli/hlt/pushf)
  NON_ARCH_NO_BARRIER=()
  for f in "${NON_ARCH_FILES[@]}"; do
    if [[ "$f" == "include/kernel/types.h" ]] || [[ "$f" == "include/kernel/assert.h" ]]; then
      continue
    fi
    NON_ARCH_NO_BARRIER+=("$f")
  done
  if [[ ${#NON_ARCH_NO_BARRIER[@]} -gt 0 ]]; then
    check_forbidden_patterns "Inline assembly (__asm__/asm) is only allowed in arch/ code (except barrier() in include/kernel/types.h)" \
      '\b(__asm__|asm)\b' "${NON_ARCH_NO_BARRIER[@]}"
  fi
fi

# 6) Static Analysis Tools (optional, warn if not available)
run_cppcheck() {
  if ! command -v cppcheck &> /dev/null; then
    echo "[check-kernel-c-style] cppcheck not found (optional)"
    return 0
  fi

  echo "[check-kernel-c-style] Running cppcheck..."

  # Run cppcheck with kernel-appropriate settings
  # --quiet: only show errors
  # --enable=warning,style,performance,portability: check categories
  # --suppress=unusedFunction: kernel has many functions called from asm
  # --inline-suppr: allow inline suppressions
  # --error-exitcode=1: return non-zero on errors
  cppcheck --quiet \
    --enable=warning,style,performance,portability \
    --suppress=unusedFunction \
    --suppress=missingIncludeSystem \
    --inline-suppr \
    --error-exitcode=1 \
    -I include \
    core/ arch/x86/ drivers/ mm/ lib/ 2>&1 | \
    grep -v "^Checking" || true

  local result=$?
  if [[ $result -ne 0 ]]; then
    warn "cppcheck found issues"
    return 1
  fi
  return 0
}

run_clang_tidy() {
  if ! command -v clang-tidy &> /dev/null; then
    echo "[check-kernel-c-style] clang-tidy not found (optional)"
    return 0
  fi

  echo "[check-kernel-c-style] Running clang-tidy..."

  # Run clang-tidy on a subset of files (it's slow)
  # Focus on core/ and mm/ for now
  local tidy_failed=0
  for file in core/*.c mm/*.c; do
    [[ -f "$file" ]] || continue

    # Run clang-tidy with kernel-appropriate checks
    # Disable checks that don't apply to kernel code
    clang-tidy "$file" \
      --checks='-*,bugprone-*,clang-analyzer-*,performance-*,-bugprone-easily-swappable-parameters' \
      --warnings-as-errors='' \
      -- -I include -nostdinc -ffreestanding &> /tmp/clang-tidy-$$.txt

    if grep -q "warning:" /tmp/clang-tidy-$$.txt; then
      cat /tmp/clang-tidy-$$.txt | grep "warning:"
      tidy_failed=1
    fi
    rm -f /tmp/clang-tidy-$$.txt
  done

  if [[ $tidy_failed -eq 1 ]]; then
    warn "clang-tidy found issues"
    return 1
  fi
  return 0
}

# Run static analysis if KERNEL_STATIC_ANALYSIS is set
if [[ "${KERNEL_STATIC_ANALYSIS:-0}" == "1" ]]; then
  run_cppcheck || EXIT=1
  run_clang_tidy || EXIT=1
fi

if [[ $EXIT -eq 0 ]]; then
  echo "[check-kernel-c-style] OK"
else
  echo ""
  echo "One or more style checks failed. See docs/KERNEL_C_STYLE.md for rules."
fi

exit "$EXIT"
