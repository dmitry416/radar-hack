#pragma once

#include "process.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

struct ModuleImage {
    std::uintptr_t base = 0;
    std::vector<std::uint8_t> bytes;
};

bool loadModule(const Process &process, const char *name, ModuleImage &image);

// Pattern syntax: "48 89 0D ? ? ? ? E9", '?' is a wildcard byte.
std::ptrdiff_t findPattern(const ModuleImage &image, const char *pattern);

// Returns the RVA targeted by the rip-relative rel32 located `relAt` bytes into the match, or 0.
std::uintptr_t findRipTarget(const ModuleImage &image, const char *pattern, std::size_t relAt);

bool findImmediate(const ModuleImage &image, const char *pattern, std::size_t immAt, std::uint32_t &out);
