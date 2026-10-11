#pragma once

#include "process.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace offsets {
    // Relative to client.dll base
    inline std::uintptr_t dwEntityList = 0;
    inline std::uintptr_t dwLocalPlayerPawn = 0;

    inline std::ptrdiff_t m_hPlayerPawn = 0;
    inline std::ptrdiff_t m_iHealth = 0;
    inline std::ptrdiff_t m_iTeamNum = 0;
    inline std::ptrdiff_t m_entitySpottedState = 0;
    inline std::ptrdiff_t m_bSpotted = 0;

    inline std::uintptr_t clientBase = 0;

    namespace detail {
        constexpr std::size_t PageSize = 0x1000;

        constexpr std::size_t HashBuckets = 256;
        constexpr std::size_t HashSize = 0x1870;
        constexpr std::size_t HashUsedOffset = 0x0C;
        constexpr std::size_t HashPeakOffset = 0x10;
        constexpr std::size_t HashBlobListOffset = 0x20;
        constexpr std::size_t HashBucketsOffset = 0x60;
        constexpr std::size_t HashBucketSize = 0x18;
        constexpr std::size_t BucketFirstOffset = 0x10;
        constexpr std::size_t NodeNextOffset = 0x8;
        constexpr std::size_t NodeDataOffset = 0x10;
        constexpr std::size_t BlobNextOffset = 0x0;
        constexpr std::size_t BlobDataOffset = 0x10;
        constexpr int ListGuard = 100000;

        constexpr std::size_t SchemaScopeCountOffset = 0x190;
        constexpr std::size_t SchemaScopesOffset = 0x198;
        constexpr std::size_t SchemaRegistrationsOffset = 0x280;
        constexpr std::size_t ScopeNameOffset = 0x8;
        constexpr std::size_t ScopeClassesOffset = 0x560;
        constexpr std::size_t ClassNameOffset = 0x8;
        constexpr std::size_t ClassFieldCountOffset = 0x24;
        constexpr std::size_t ClassFieldsOffset = 0x30;
        constexpr std::size_t FieldSize = 0x20;
        constexpr std::size_t FieldNameOffset = 0x0;
        constexpr std::size_t FieldOffsetOffset = 0x10;

        inline std::string &error() {
            static std::string message;
            return message;
        }

        inline bool fail(std::string message) {
            error() = std::move(message);
            return false;
        }

        struct ModuleImage {
            std::uintptr_t base = 0;
            std::vector<std::uint8_t> bytes;
        };

        inline bool loadModule(const Process &process, const char *name, ModuleImage &image) {
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

        // Pattern syntax: "48 89 0D ? ? ? ? E9", '?' is a wildcard byte.
        inline std::ptrdiff_t scan(const ModuleImage &image, const char *pattern) {
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

        // Resolves the rip-relative rel32 found `relAt` bytes into the match; returns the target RVA.
        inline std::uintptr_t scanRip(const ModuleImage &image, const char *pattern, std::size_t relAt) {
            const std::ptrdiff_t pos = scan(image, pattern);
            if (pos < 0) return 0;

            std::int32_t rel;
            std::memcpy(&rel, image.bytes.data() + pos + relAt, sizeof(rel));
            return static_cast<std::uintptr_t>(static_cast<std::int64_t>(pos + relAt + 4) + rel);
        }

        inline bool scanU32(const ModuleImage &image, const char *pattern, std::size_t immAt,
                            std::uint32_t &out) {
            const std::ptrdiff_t pos = scan(image, pattern);
            if (pos < 0) return false;

            std::memcpy(&out, image.bytes.data() + pos + immAt, sizeof(out));
            return true;
        }

        inline std::vector<std::uintptr_t> hashElements(const Process &process, std::uintptr_t hashAddress) {
            std::vector<std::uint8_t> hash(HashSize);
            std::vector<std::uintptr_t> result;
            if (!process.readBuffer(hashAddress, hash.data(), hash.size())) return result;

            auto i32 = [&](std::size_t offset) {
                std::int32_t value;
                std::memcpy(&value, &hash[offset], sizeof(value));
                return value;
            };
            auto u64 = [&](std::size_t offset) {
                std::uintptr_t value;
                std::memcpy(&value, &hash[offset], sizeof(value));
                return value;
            };

            std::unordered_set<std::uintptr_t> seen;
            auto push = [&](std::uintptr_t pointer) {
                if (pointer && seen.insert(pointer).second) result.push_back(pointer);
            };

            const std::size_t used = static_cast<std::size_t>(i32(HashUsedOffset));
            std::size_t count = 0;
            for (std::size_t b = 0; b < HashBuckets && count < used; ++b) {
                std::uintptr_t node = u64(HashBucketsOffset + b * HashBucketSize + BucketFirstOffset);
                for (int guard = 0; node && guard < ListGuard; ++guard) {
                    const auto next = process.read<std::uintptr_t>(node + NodeNextOffset);
                    const auto data = process.read<std::uintptr_t>(node + NodeDataOffset);
                    if (!next || !data) break;

                    if (*data) {
                        push(*data);
                        ++count;
                    }
                    if (count >= used) break;
                    node = *next;
                }
            }

            const std::size_t peak = static_cast<std::size_t>(i32(HashPeakOffset));
            std::uintptr_t blob = u64(HashBlobListOffset);
            std::size_t blobCount = 0;
            for (int guard = 0; blob && guard < ListGuard; ++guard) {
                const auto next = process.read<std::uintptr_t>(blob + BlobNextOffset);
                const auto data = process.read<std::uintptr_t>(blob + BlobDataOffset);
                if (!next || !data) break;

                if (*data) {
                    push(*data);
                    ++blobCount;
                }
                if (blobCount >= peak) break;
                blob = *next;
            }
            return result;
        }

        struct FieldRequest {
            const char *className;
            const char *fieldName;
            std::ptrdiff_t *target;
            bool found = false;
        };

        inline bool resolveNetvars(const Process &process, const ModuleImage &schemaSystem,
                                   FieldRequest *requests, std::size_t requestCount) {
            const std::uintptr_t rva = scanRip(schemaSystem, "4C 8D 35 ? ? ? ? 0F 28 45", 3);
            if (!rva) return fail("schemasystem pattern is outdated");
            const std::uintptr_t system = schemaSystem.base + rva;

            std::int32_t scopeCount = 0, registrations = 0;
            std::uintptr_t scopes = 0;
            if (!process.readBuffer(system + SchemaScopeCountOffset, &scopeCount, sizeof(scopeCount)) ||
                !process.readBuffer(system + SchemaScopesOffset, &scopes, sizeof(scopes)) ||
                !process.readBuffer(system + SchemaRegistrationsOffset, &registrations, sizeof(registrations)))
                return fail("failed to read SchemaSystem");
            if (registrations == 0) return fail("schema system has no registrations yet (game still loading?)");

            std::size_t remaining = requestCount;
            for (std::int32_t i = 0; i < scopeCount && remaining; ++i) {
                const auto scope = process.read<std::uintptr_t>(scopes + static_cast<std::uintptr_t>(i) * 8);
                if (!scope || process.readString(*scope + ScopeNameOffset, 64) != "client.dll") continue;

                for (std::uintptr_t cls: hashElements(process, *scope + ScopeClassesOffset)) {
                    if (!remaining) break;

                    const auto namePointer = process.read<std::uintptr_t>(cls + ClassNameOffset);
                    if (!namePointer) continue;
                    const std::string className = process.readString(*namePointer, 128);

                    for (std::size_t r = 0; r < requestCount; ++r) {
                        if (requests[r].found || className != requests[r].className) continue;

                        const auto fieldCount = process.read<std::int16_t>(cls + ClassFieldCountOffset);
                        const auto fields = process.read<std::uintptr_t>(cls + ClassFieldsOffset);
                        if (!fieldCount || !fields) continue;

                        for (std::int16_t f = 0; f < *fieldCount; ++f) {
                            const std::uintptr_t field = *fields + static_cast<std::uintptr_t>(f) * FieldSize;
                            const auto fieldName = process.read<std::uintptr_t>(field + FieldNameOffset);
                            const auto fieldOffset = process.read<std::int32_t>(field + FieldOffsetOffset);
                            if (!fieldName || !fieldOffset) continue;

                            if (process.readString(*fieldName, 128) == requests[r].fieldName) {
                                *requests[r].target = *fieldOffset;
                                requests[r].found = true;
                                --remaining;
                                break;
                            }
                        }
                    }
                }
                break;
            }

            for (std::size_t r = 0; r < requestCount; ++r) {
                if (!requests[r].found)
                    return fail(std::string("netvar not found: ") + requests[r].className +
                                "::" + requests[r].fieldName);
            }
            return true;
        }
    }

    inline const std::string &lastError() {
        return detail::error();
    }

    inline bool init(const Process &process) {
        using namespace detail;

        ModuleImage client, schema;
        if (!loadModule(process, "client.dll", client)) return fail("client.dll not loaded");
        if (!loadModule(process, "schemasystem.dll", schema)) return fail("schemasystem.dll not loaded");
        clientBase = client.base;

        dwEntityList = scanRip(client, "48 89 0D ? ? ? ? E9 ? ? ? ? CC", 3);
        if (!dwEntityList) return fail("dwEntityList pattern is outdated");

        // dwLocalPlayerPawn = CPrediction global + the pawn field offset embedded in a prediction routine
        const std::uintptr_t prediction =
                scanRip(client, "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54", 3);
        std::uint32_t pawnInPrediction = 0;
        if (!prediction || !scanU32(client, "4C 39 B6 ? ? ? ? 74 ? 44 88 BE", 3, pawnInPrediction))
            return fail("dwLocalPlayerPawn pattern is outdated");
        dwLocalPlayerPawn = prediction + pawnInPrediction;

        FieldRequest requests[] = {
            {"CCSPlayerController", "m_hPlayerPawn", &m_hPlayerPawn},
            {"C_BaseEntity", "m_iHealth", &m_iHealth},
            {"C_BaseEntity", "m_iTeamNum", &m_iTeamNum},
            {"C_CSPlayerPawn", "m_entitySpottedState", &m_entitySpottedState},
            {"EntitySpottedState_t", "m_bSpotted", &m_bSpotted},
        };
        return resolveNetvars(process, schema, requests, sizeof(requests) / sizeof(requests[0]));
    }
}
