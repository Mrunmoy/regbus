#pragma once

// regbus version header (header-only; no deps).
// Update these macros when you cut a release tag.
#define REGBUS_VERSION_MAJOR 0
#define REGBUS_VERSION_MINOR 1
#define REGBUS_VERSION_PATCH 1
#define REGBUS_VERSION_STRING "0.1.1"

// 0xMMmmpp -> e.g., 0.1.0 -> 0x000100
#define REGBUS_VERSION_HEX ((REGBUS_VERSION_MAJOR << 16) | (REGBUS_VERSION_MINOR << 8) | (REGBUS_VERSION_PATCH))

// Compile-time checks
#define REGBUS_VERSION_AT_LEAST(MAJ, MIN, PAT)                          \
    ((REGBUS_VERSION_MAJOR > (MAJ)) ||                                  \
     (REGBUS_VERSION_MAJOR == (MAJ) && REGBUS_VERSION_MINOR > (MIN)) || \
     (REGBUS_VERSION_MAJOR == (MAJ) && REGBUS_VERSION_MINOR == (MIN) && REGBUS_VERSION_PATCH >= (PAT)))

namespace regbus
{
    constexpr int version_major = REGBUS_VERSION_MAJOR;
    constexpr int version_minor = REGBUS_VERSION_MINOR;
    constexpr int version_patch = REGBUS_VERSION_PATCH;
    constexpr unsigned version_hex = REGBUS_VERSION_HEX;
    constexpr const char *version_string = REGBUS_VERSION_STRING;
}
