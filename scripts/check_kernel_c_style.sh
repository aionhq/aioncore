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
  # Allow the canonical barrier() macro in include/kernel/types.h
  NON_ARCH_NO_BARRIER=()
  for f in "${NON_ARCH_FILES[@]}"; do
    if [[ "$f" == "include/kernel/types.h" ]]; then
      continue
    fi
    NON_ARCH_NO_BARRIER+=("$f")
  done
  if [[ ${#NON_ARCH_NO_BARRIER[@]} -gt 0 ]]; then
    check_forbidden_patterns "Inline assembly (__asm__/asm) is only allowed in arch/ code (except barrier() in include/kernel/types.h)" \
      '\b(__asm__|asm)\b' "${NON_ARCH_NO_BARRIER[@]}"
  fi
fi

if [[ $EXIT -eq 0 ]]; then
  echo "[check-kernel-c-style] OK"
else
  echo ""
  echo "One or more style checks failed. See docs/KERNEL_C_STYLE.md for rules."
fi

exit "$EXIT"
