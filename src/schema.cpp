#include "schema.hpp"

#include <cstring>
#include <unordered_set>

namespace {
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

    constexpr const char *SchemaSystemPattern = "4C 8D 35 ? ? ? ? 0F 28 45";
    constexpr const char *ClientScopeName = "client.dll";

    std::vector<std::uintptr_t> hashElements(const Process &process, std::uintptr_t hashAddress) {
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

        const auto used = static_cast<std::size_t>(i32(HashUsedOffset));
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

        const auto peak = static_cast<std::size_t>(i32(HashPeakOffset));
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

    bool findField(const Process &process, std::uintptr_t cls, const char *fieldName, std::ptrdiff_t &offset) {
        const auto fieldCount = process.read<std::int16_t>(cls + ClassFieldCountOffset);
        const auto fields = process.read<std::uintptr_t>(cls + ClassFieldsOffset);
        if (!fieldCount || !fields) return false;

        for (std::int16_t i = 0; i < *fieldCount; ++i) {
            const std::uintptr_t field = *fields + static_cast<std::uintptr_t>(i) * FieldSize;
            const auto namePointer = process.read<std::uintptr_t>(field + FieldNameOffset);
            const auto fieldOffset = process.read<std::int32_t>(field + FieldOffsetOffset);
            if (!namePointer || !fieldOffset) continue;

            if (process.readString(*namePointer, 128) == fieldName) {
                offset = *fieldOffset;
                return true;
            }
        }
        return false;
    }
}

bool resolveNetvars(const Process &process, const ModuleImage &schemaSystem,
                    const std::vector<NetvarRequest> &requests, std::string &error) {
    const std::uintptr_t rva = findRipTarget(schemaSystem, SchemaSystemPattern, 3);
    if (!rva) {
        error = "schemasystem pattern is outdated";
        return false;
    }
    const std::uintptr_t system = schemaSystem.base + rva;

    std::int32_t scopeCount = 0, registrations = 0;
    std::uintptr_t scopes = 0;
    if (!process.readBuffer(system + SchemaScopeCountOffset, &scopeCount, sizeof(scopeCount)) ||
        !process.readBuffer(system + SchemaScopesOffset, &scopes, sizeof(scopes)) ||
        !process.readBuffer(system + SchemaRegistrationsOffset, &registrations, sizeof(registrations))) {
        error = "failed to read SchemaSystem";
        return false;
    }
    if (registrations == 0) {
        error = "schema system has no registrations yet (game still loading?)";
        return false;
    }

    std::vector<bool> found(requests.size(), false);
    std::size_t remaining = requests.size();

    for (std::int32_t i = 0; i < scopeCount && remaining; ++i) {
        const auto scope = process.read<std::uintptr_t>(scopes + static_cast<std::uintptr_t>(i) * sizeof(std::uintptr_t));
        if (!scope || process.readString(*scope + ScopeNameOffset, 64) != ClientScopeName) continue;

        for (std::uintptr_t cls: hashElements(process, *scope + ScopeClassesOffset)) {
            if (!remaining) break;

            const auto namePointer = process.read<std::uintptr_t>(cls + ClassNameOffset);
            if (!namePointer) continue;
            const std::string className = process.readString(*namePointer, 128);

            for (std::size_t r = 0; r < requests.size(); ++r) {
                if (found[r] || className != requests[r].className) continue;

                if (findField(process, cls, requests[r].fieldName, *requests[r].target)) {
                    found[r] = true;
                    --remaining;
                }
            }
        }
        break;
    }

    for (std::size_t r = 0; r < requests.size(); ++r) {
        if (!found[r]) {
            error = std::string("netvar not found: ") + requests[r].className + "::" + requests[r].fieldName;
            return false;
        }
    }
    return true;
}
