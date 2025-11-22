// Kernel build-time configuration profiles
//
// This header defines a small set of profiles and derives feature flags
// from them. The idea is:
//   - DEV:       maximal debug / checks
//   - STANDARD:  sane defaults, minimal overhead
//   - HARDENED:  extra robustness features enabled
//
// The build system can define CONFIG_PROFILE_DEV or CONFIG_PROFILE_HARDENED.
// If none is defined, CONFIG_PROFILE_STANDARD is used by default.

#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

// ---------------------------------------------------------------------------
// Profile selection
// ---------------------------------------------------------------------------

#if !defined(CONFIG_PROFILE_DEV) && \
    !defined(CONFIG_PROFILE_STANDARD) && \
    !defined(CONFIG_PROFILE_HARDENED)
// Default profile if none is specified
#define CONFIG_PROFILE_STANDARD 1
#endif

// ---------------------------------------------------------------------------
// Derived feature flags
// ---------------------------------------------------------------------------

#if defined(CONFIG_PROFILE_DEV)

#define CONFIG_ENABLE_DEBUG_ASSERTS      1
#define CONFIG_ENABLE_PARANOID_CHECKS    1
#define CONFIG_ENABLE_LOG_VERBOSE        1
#define CONFIG_ENABLE_HASH_KERNEL_TEXT   1
#define CONFIG_ENABLE_RAM_SCRUBBER       1

#elif defined(CONFIG_PROFILE_HARDENED)

#define CONFIG_ENABLE_DEBUG_ASSERTS      0
#define CONFIG_ENABLE_PARANOID_CHECKS    1
#define CONFIG_ENABLE_LOG_VERBOSE        0
#define CONFIG_ENABLE_HASH_KERNEL_TEXT   1
#define CONFIG_ENABLE_RAM_SCRUBBER       1

#elif defined(CONFIG_PROFILE_STANDARD)

#define CONFIG_ENABLE_DEBUG_ASSERTS      0
#define CONFIG_ENABLE_PARANOID_CHECKS    0
#define CONFIG_ENABLE_LOG_VERBOSE        0
#define CONFIG_ENABLE_HASH_KERNEL_TEXT   0
#define CONFIG_ENABLE_RAM_SCRUBBER       1   // Scrubber exists, can be slow/low rate

#else
#error "Unknown CONFIG_PROFILE_* selection"
#endif

#endif // KERNEL_CONFIG_H

