# Debug Serial Cleanup Plan

This document captures how we are using serial debug output today and how we plan to clean it up once the kernel is more mature.

The goal is to:
- Keep debug serial **powerful for bring-up** and low-level debugging.
- Avoid letting debug code **pollute core kernel paths** in the long term.
- Make it easy to **strip or minimize** serial debugging for production/RT builds.

---

## 1. Current State (2025-11-22)

### 1.1 Debug Backend

- `drivers/serial/serial.c`, `drivers/serial/serial.h`:
  - Simple, polled UART driver for COM1/COM2.
  - Best-effort, non-blocking (small polling loop + timeout).
- `include/kernel/debug.h` and `core/debug.c`:
  - Provide `debug_serial_init()` and `debug_serial_printf()` behind a `DEBUG_SERIAL` macro.
  - Define `dlog(fmt, ...)`:
    - If `DEBUG_SERIAL` is defined: prints to serial.
    - Otherwise: compiles to a no-op.

### 1.2 Call Sites

- `core/init.c`:
  - Uses `dlog()` for boot tracing:
    - Entry into `kmain`.
    - Multiboot parameters.
    - HAL, PMM, MMU initialization milestones.

### 1.3 Build Behaviour

- In normal builds (no `DEBUG_SERIAL`), debug serial code is compiled out (only a tiny static cost for the headers).
- In debug builds (`DEBUG_SERIAL` defined via `CFLAGS`), `debug_serial_init()` and `dlog()` are available and can be used for deep bring-up and diagnostics.

---

## 2. Clean-Up Goals

When the kernel matures (post-Phase 3/4), we want:

1. **Zero debug serial in RT-critical paths**:
   - No `dlog()` in interrupt handlers, scheduler, IPC fast paths, or MM hot paths.
2. **Minimal boot-time serial**:
   - Only a handful of critical messages (e.g., early panic, boot banner, major failures).
3. **Configurable debug level**:
   - Ability to reduce or disable serial chatter without editing code:
     - Build-time: `DEBUG_SERIAL` off.
     - Run-time (future): simple log level (ERROR/WARN/INFO/DEBUG).
4. **Clear separation between “debug kernel” and “production kernel”**:
   - Production kernel:
     - `DEBUG_SERIAL` off.
     - Assertions mostly off.
     - Minimal or no serial output.
   - Debug kernel:
     - `DEBUG_SERIAL` on.
     - Assertions enabled.
     - Extra tracing/logging for development.

---

## 3. Action Items for Future Cleanup

These items are **not urgent now** but should be addressed as we move into later phases (tasks, IPC, units).

### 3.1 Audit and Trim `dlog()` Call Sites

- [ ] Audit all uses of `dlog()` with `rg "dlog\(" -n core mm arch drivers`.
- [ ] Remove or downgrade high-frequency logs:
  - PMM/bitmap spam during normal boot.
  - MMU mapping spam during identity map.
- [ ] Keep only:
  - High-value boot markers (`kmain` entry, paging enabled, PMM/MMU summaries).
  - Early panic paths.

### 3.2 Move Heavy Tracing to Per-CPU Trace Buffers

- [ ] For repeated state snapshots (e.g., PMM stats, scheduler state), use:
  - `trace_event()` into per-CPU buffers.
  - A low-priority debug thread/unit to drain trace buffers to serial if needed.
- This keeps RT paths clean while still allowing deep diagnostics.

### 3.3 Introduce a Simple Log Level (Optional)

- [ ] Add a compile-time or global log level:
  - `DEBUG_LOG_LEVEL_NONE`
  - `DEBUG_LOG_LEVEL_ERROR`
  - `DEBUG_LOG_LEVEL_INFO`
  - `DEBUG_LOG_LEVEL_DEBUG`
- [ ] Wrap `dlog()` in a macro that checks the log level before printing:

```c
#ifdef DEBUG_SERIAL
extern int debug_log_level;
#define dlog_if(level, fmt, ...) \
    do { if ((level) <= debug_log_level) debug_serial_printf(fmt, ##__VA_ARGS__); } while (0)
#else
#define dlog_if(level, fmt, ...) ((void)0)
#endif
```

- [ ] Update existing `dlog()` call sites to use `dlog_if(DEBUG_LOG_LEVEL_*, ...)` where appropriate.

### 3.4 Ensure Serial Driver is Optional

- [ ] In the Makefile:
  - Only build `drivers/serial/serial.c` and `core/debug.c` if `DEBUG_SERIAL` is defined.
- [ ] In headers:
  - Keep serial driver API (`serial_init`, `serial_write`, etc.) scoped to debug builds or explicit users (e.g., early-stage console servers).

---

## 4. When to Revisit This

We should revisit this file and clean up debug serial when:

- Phase 3 (Tasks, Scheduling & Syscalls) is stable and we can:
  - Context switch reliably.
  - Run a basic userspace task.
- Phase 4 (IPC & Capabilities) is underway, and:
  - We rely more heavily on units, channels, and capabilities for observability.

At that point, we can:

- Trim `dlog()` usage.
- Optionally move most debugging to userspace (e.g., debug units/servers reading trace buffers).
- Reserve serial for:
  - Very early boot.
  - Panic/debug paths.

---

## 5. Quick Checklist for Serial Debug Hygiene

Before shipping a “stable” or RT-focused build:

- [ ] `DEBUG_SERIAL` disabled in production build configs.
- [ ] No `dlog()` calls in RT-critical code paths.
- [ ] Serial driver only built when explicitly requested.
- [ ] Panic/ASSERT paths can still optionally mirror to serial (under DEBUG or special builds).

This log is a reminder that serial debug is a **temporary crutch**, not a permanent API surface. The long-term plan is to push most debug/trace functionality into:

- Per-CPU trace buffers.
- Units/services that read and analyze those traces.
- Structured logging exposed via capabilities and IPC.

