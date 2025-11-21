# Documentation Guide

This document explains how all the kernel documentation fits together.

## Document Hierarchy

### 🎯 Primary Guiding Documents

These are the **main** documents that guide all development:

1. **[VISION.md](VISION.md)** - The North Star
   - Long-term architectural vision
   - Core abstractions: units, threads, capabilities, channels
   - What kernel owns vs userspace
   - Draft ABI specifications
   - Aspirational features (cluster, P2P, hooks)
   - **Use this for:** Understanding "why" and "what" we're building

2. **[IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md)** - The Execution Plan
   - Phase-by-phase breakdown (currently Phase 2)
   - Concrete APIs and function signatures
   - RT constraints per component
   - Milestones and completion status
   - **Use this for:** Understanding "how" and "when" we're building it

### 📚 Supporting Detail Documents

These provide deep-dives into specific topics:

3. **[ARCHITECTURE.md](ARCHITECTURE.md)** - Design Principles
   - Historical caveats (what went wrong in other kernels)
   - Modern principles (microkernel, per-CPU, capability-based)
   - Component boundaries
   - Memory safety patterns
   - **Use this for:** Understanding design decisions and trade-offs

4. **[UNITS_ARCHITECTURE.md](UNITS_ARCHITECTURE.md)** - Units Model Details
   - Detailed unit/thread/channel/capability structs
   - API design for each abstraction
   - Example: spawning first userspace unit
   - Implementation phases
   - **Use this for:** Deep dive into the units execution model

5. **[RT_CONSTRAINTS.md](RT_CONSTRAINTS.md)** - Real-Time Requirements
   - Timing budgets for all operations
   - O(1) requirements and examples
   - Priority inheritance protocols
   - Good vs bad code patterns
   - Testing determinism
   - **Use this for:** Ensuring RT correctness

6. **[FORMAL_VERIFICATION.md](FORMAL_VERIFICATION.md)** - Verification Strategy
   - Determinism guarantees (100% achievable)
   - Formal verification levels (partial achievable)
   - Tools and techniques
   - Testing strategies
   - **Use this for:** Building trustworthy, verifiable code

7. **[MULTI_ARCH.md](MULTI_ARCH.md)** - Multi-Architecture Support
   - HAL portability strategy
   - RISC-V as second architecture
   - Per-architecture effort estimates
   - Portable coding guidelines
   - **Use this for:** Keeping code architecture-neutral

8. **[KERNEL_C_STYLE.md](KERNEL_C_STYLE.md)** - C Coding Standards
   - Allowed C subset and forbidden constructs
   - Memory safety and ownership rules
   - Concurrency and RT rules
   - Module structure and headers
   - Error handling patterns
   - Style and naming conventions
   - **Use this for:** Writing correct, safe kernel C code

9. **[TESTING.md](TESTING.md)** - Unit Testing Guide
   - ktest framework usage
   - Writing and registering tests
   - Assertion macros
   - Building and running tests
   - Testing RT-critical code
   - **Use this for:** Writing unit tests for kernel subsystems

10. **[ISSUES.md](ISSUES.md)** - Known Issues & Action Items
    - Critical issues and fixes
    - Technical debt tracking
    - Priority and status
    - Estimated effort
    - **Use this for:** Tracking what needs fixing

### 📋 Reference Documents

11. **README.md** - Quick Start
    - Build instructions
    - Running in QEMU
    - Basic overview
    - Links to other docs

12. **GETTING_STARTED.md** - (TODO)
    - Development environment setup
    - First build walkthrough
    - Debugging tips

---

## How to Use These Documents

### Starting a New Feature

1. Check **VISION.md** - Does this feature align with the vision?
2. Check **IMPLEMENTATION_ROADMAP.md** - Which phase is it in?
3. Check relevant detail docs (e.g., **UNITS_ARCHITECTURE.md** for unit-related work)
4. Check **RT_CONSTRAINTS.md** - What are the timing requirements?
5. Check **ISSUES.md** - Are there related issues?

### During Implementation

1. Follow **IMPLEMENTATION_ROADMAP.md** for API signatures
2. Follow **RT_CONSTRAINTS.md** for performance requirements
3. Follow **FORMAL_VERIFICATION.md** for assertion/invariant patterns
4. Update **ISSUES.md** if you discover problems

### After Completion

1. Update **IMPLEMENTATION_ROADMAP.md** - Mark milestone complete
2. Update **ISSUES.md** - Close resolved issues
3. If ABI changed, update **VISION.md** section 14
4. If principles emerged, update **ARCHITECTURE.md**

---

## Document Sync Rules

### Primary Documents (Must Stay in Sync)

**VISION.md** and **IMPLEMENTATION_ROADMAP.md** must align:
- APIs in ROADMAP must match draft ABIs in VISION
- Phase goals in ROADMAP must map to VISION concepts
- Completion in ROADMAP feeds into VISION relation (section 13)

**How to sync:**
- When changing VISION ABI → update ROADMAP APIs
- When completing ROADMAP phase → update VISION relation section
- When adding new concept → add to both (vision first, then implementation plan)

### Supporting Documents

Supporting docs **derive from** primary documents:
- UNITS_ARCHITECTURE.md elaborates VISION section 1 (units/threads)
- RT_CONSTRAINTS.md implements ROADMAP philosophy (RT requirements)
- MULTI_ARCH.md extends ARCHITECTURE.md (HAL portability)

**How to sync:**
- When VISION/ROADMAP changes → update relevant detail docs
- Detail docs should reference primary docs (e.g., "See VISION.md section X")
- If detail doc discovers issue → escalate to VISION/ROADMAP if architectural

### Issues Document

**ISSUES.md** is ephemeral and tracks current work:
- Gets updated constantly during development
- Issues move from "TODO" → "IN PROGRESS" → "FIXED"
- Fixed issues get archived in "Resolved Issues" section
- Major architectural issues may warrant ARCHITECTURE.md updates

---

## Document Relationships (Visual)

```
┌─────────────────────────────────────────────────┐
│                   VISION.md                     │
│         (What we're building & why)             │
│    • Units, threads, capabilities, channels     │
│    • Draft ABI specifications                   │
│    • Long-term features                         │
└──────────────────┬──────────────────────────────┘
                   │
                   │ Guides
                   ▼
┌─────────────────────────────────────────────────┐
│          IMPLEMENTATION_ROADMAP.md              │
│       (How & when we're building it)            │
│    • Phase 1 (DONE), Phase 2 (IN PROGRESS)     │
│    • Concrete APIs and milestones              │
│    • RT constraints per component              │
└──────────────────┬──────────────────────────────┘
                   │
                   │ Detailed by
                   ▼
        ┌──────────┴──────────┐
        │                     │
        ▼                     ▼
┌─────────────────┐   ┌─────────────────┐
│UNITS_            │   │RT_CONSTRAINTS   │
│ARCHITECTURE.md   │   │.md              │
│• Unit model      │   │• Timing budgets │
│• API details     │   │• O(1) patterns  │
└─────────────────┘   └─────────────────┘
        ▼                     ▼
┌─────────────────┐   ┌─────────────────┐
│FORMAL_           │   │MULTI_ARCH.md    │
│VERIFICATION.md   │   │• HAL portability│
│• Test strategies │   │• RISC-V plan    │
└─────────────────┘   └─────────────────┘
                   ▲
                   │
                   │ Tracked by
                   │
        ┌──────────┴──────────┐
        │     ISSUES.md        │
        │  • Current problems  │
        │  • Action items      │
        └──────────────────────┘
```

---

## Version Control & Updates

### When to Update Each Document

| Document | Update Frequency | Update Trigger |
|----------|------------------|----------------|
| **VISION.md** | Rarely (major changes only) | New abstraction, ABI change, paradigm shift |
| **IMPLEMENTATION_ROADMAP.md** | Weekly/Per-phase | Complete milestone, start new phase |
| **UNITS_ARCHITECTURE.md** | As needed | Units API evolves |
| **RT_CONSTRAINTS.md** | As needed | New RT requirements discovered |
| **FORMAL_VERIFICATION.md** | Monthly | Verification approach changes |
| **MULTI_ARCH.md** | When porting | Add new architecture |
| **ISSUES.md** | Daily | Discover issue, fix issue, change priority |
| **ARCHITECTURE.md** | Rarely | Major design principle emerges |

### Keeping Documents in Sync

**Weekly sync check:**
```bash
# Check that completed work in ROADMAP is reflected
grep "✅" IMPLEMENTATION_ROADMAP.md
# → Should see all completed phases/items

# Check that VISION relation section is current
grep "current codebase" VISION.md
# → Should mention what's actually implemented

# Check for stale TODOs in detail docs
grep -r "TODO" *.md | grep -v ISSUES.md
# → Should be minimal, most TODOs in ISSUES.md
```

---

## Current Status (2025-11-21)

### Implementation Status

- ✅ **Phase 1:** Foundation (HAL, per-CPU, IDT, VGA) - **COMPLETE**
- 🔨 **Phase 2:** Timer, memory, paging - **IN PROGRESS**
  - ✅ IDT and interrupts ready
  - ✅ PIT timer with TSC calibration complete
  - ✅ Unit testing framework operational
  - 🔨 PMM (Physical Memory Manager) next
  - 📋 Basic paging after PMM

### Document Status

- ✅ **VISION.md** - Comprehensive, stable
- ✅ **IMPLEMENTATION_ROADMAP.md** - Up-to-date, Phase 2 in progress
- ✅ **ARCHITECTURE.md** - Solid design principles documented
- ✅ **UNITS_ARCHITECTURE.md** - Detailed unit model defined
- ✅ **RT_CONSTRAINTS.md** - RT requirements clear
- ✅ **FORMAL_VERIFICATION.md** - Verification strategy defined
- ✅ **MULTI_ARCH.md** - RISC-V port strategy documented
- ✅ **KERNEL_C_STYLE.md** - C style guide and rules complete
- ✅ **TESTING.md** - Unit testing guide complete
- ✅ **ISSUES.md** - Current issues tracked
- ✅ **README.md** - Updated to reflect modern architecture
- 📋 **GETTING_STARTED.md** - TODO

---

## Quick Navigation

**Want to understand the big picture?**
→ Read [VISION.md](VISION.md)

**Want to know what's next?**
→ Read [IMPLEMENTATION_ROADMAP.md](IMPLEMENTATION_ROADMAP.md) or [CURRENT_WORK.md](../CURRENT_WORK.md)

**Want to understand why we made certain decisions?**
→ Read [ARCHITECTURE.md](ARCHITECTURE.md)

**Want to implement units/threads/channels?**
→ Read [UNITS_ARCHITECTURE.md](UNITS_ARCHITECTURE.md)

**Want to ensure RT correctness?**
→ Read [RT_CONSTRAINTS.md](RT_CONSTRAINTS.md)

**Want to write kernel C code?**
→ Read [KERNEL_C_STYLE.md](KERNEL_C_STYLE.md)

**Want to write unit tests?**
→ Read [TESTING.md](TESTING.md)

**Want to verify the kernel?**
→ Read [FORMAL_VERIFICATION.md](FORMAL_VERIFICATION.md)

**Want to port to RISC-V?**
→ Read [MULTI_ARCH.md](MULTI_ARCH.md)

**Want to see what needs fixing?**
→ Read [ISSUES.md](ISSUES.md)

---

Last Updated: 2025-11-21
