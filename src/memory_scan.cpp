#include "memory_scan.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    constexpr std::size_t PageSize = 0x1000;
}

bool loadModule(const Process &process, const char *name, ModuleImage &image) {
    ModuleInfo info;
    if (!process.findModule(name, info)) return false;

    image.base = info.base;
    image.bytes.assign(info.size, 0);
    for (std::size_t offset = 0; offset < info.size; offset += PageSize) {
        const std::size_t chunk = (std::min)(PageSize, info.size - offset);
        process.readBuffer(info.base + offset, image.bytes.data() + offset, chunk);
    }
    return true;
}

std::ptrdiff_t findPattern(const ModuleImage &image, const char *pattern) {
    std::vector<int> bytes;
    for (const char *p = pattern; *p;) {
        if (*p == ' ') {
            ++p;
        } else if (*p == '?') {
            bytes.push_back(-1);
            while (*p == '?') ++p;
        } else {
            bytes.push_back(static_cast<int>(std::strtoul(std::string(p, 2).c_str(), nullptr, 16)));
            p += 2;
        }
    }

    const auto &data = image.bytes;
    if (bytes.empty() || data.size() < bytes.size()) return -1;

    for (std::size_t i = 0; i + bytes.size() <= data.size(); ++i) {
        std::size_t j = 0;
        while (j < bytes.size() && (bytes[j] < 0 || data[i + j] == bytes[j])) ++j;
        if (j == bytes.size()) return static_cast<std::ptrdiff_t>(i);
    }
    return -1;
}

std::uintptr_t findRipTarget(const ModuleImage &image, const char *pattern, std::size_t relAt) {
    const std::ptrdiff_t pos = findPattern(image, pattern);
    if (pos < 0) return 0;

    std::int32_t rel;
    std::memcpy(&rel, image.bytes.data() + pos + relAt, sizeof(rel));
    return static_cast<std::uintptr_t>(static_cast<std::int64_t>(pos + relAt + 4) + rel);
}

bool findImmediate(const ModuleImage &image, const char *pattern, std::size_t immAt, std::uint32_t &out) {
    const std::ptrdiff_t pos = findPattern(image, pattern);
    if (pos < 0) return false;

    std::memcpy(&out, image.bytes.data() + pos + immAt, sizeof(out));
    return true;
}
