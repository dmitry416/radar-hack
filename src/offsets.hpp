#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace offsets {

// results (valid after a successful init())

// relative to client.dll base
inline std::uintptr_t dwEntityList = 0;
inline std::uintptr_t dwLocalPlayerPawn = 0;

// CCSPlayerController
inline std::ptrdiff_t m_hPlayerPawn = 0;

// C_BaseEntity / C_CSPlayerPawn
inline std::ptrdiff_t m_iHealth = 0;
inline std::ptrdiff_t m_iTeamNum = 0;
inline std::ptrdiff_t m_entitySpottedState = 0;

// EntitySpottedState_t
inline std::ptrdiff_t m_bSpotted = 0;

// process info

inline DWORD pid = 0;
inline HANDLE process = nullptr;
inline std::uintptr_t client_base = 0;

namespace detail {

inline std::string& error() {
    static std::string e;
    return e;
}

inline bool fail(std::string msg) {
    error() = std::move(msg);
    return false;
}

template <typename T>
inline bool read_raw(std::uintptr_t addr, T& out) {
    SIZE_T n = 0;
    return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(addr), &out, sizeof(T), &n) &&
           n == sizeof(T);
}

inline bool read_buf(std::uintptr_t addr, void* out, std::size_t size) {
    SIZE_T n = 0;
    return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(addr), out, size, &n) && n == size;
}

inline std::string read_cstr(std::uintptr_t addr, std::size_t max = 256) {
    std::string s(max, '\0');
    SIZE_T n = 0;
    ReadProcessMemory(process, reinterpret_cast<LPCVOID>(addr), s.data(), max, &n);
    s.resize(std::strlen(s.c_str()));
    return s;
}

struct Module {
    std::uintptr_t base = 0;
    std::size_t size = 0;
    std::vector<std::uint8_t> image;  // local copy
};

inline DWORD find_pid(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    DWORD result = 0;
    if (Process32FirstW(snap, &e)) {
        do {
            if (_wcsicmp(e.szExeFile, name) == 0) {
                result = e.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &e));
    }
    CloseHandle(snap);
    return result;
}

inline bool load_module(const wchar_t* name, Module& m) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W e{};
    e.dwSize = sizeof(e);
    bool found = false;
    if (Module32FirstW(snap, &e)) {
        do {
            if (_wcsicmp(e.szModule, name) == 0) {
                m.base = reinterpret_cast<std::uintptr_t>(e.modBaseAddr);
                m.size = e.modBaseSize;
                found = true;
                break;
            }
        } while (Module32NextW(snap, &e));
    }
    CloseHandle(snap);
    if (!found) return false;

    // read page by page so unreadable pages don't fail the whole copy
    m.image.assign(m.size, 0);
    for (std::size_t off = 0; off < m.size; off += 0x1000) {
        std::size_t chunk = (std::min)(static_cast<std::size_t>(0x1000), m.size - off);
        SIZE_T n = 0;
        ReadProcessMemory(process, reinterpret_cast<LPCVOID>(m.base + off), m.image.data() + off,
                          chunk, &n);
    }
    return true;
}

// Pattern: "48 89 0D ? ? ? ? E9" ('?' / '??' = wildcard). Returns offset of the match or -1.
inline std::ptrdiff_t scan(const Module& m, const char* pattern) {
    std::vector<int> pat;
    for (const char* p = pattern; *p;) {
        if (*p == ' ') {
            ++p;
            continue;
        }
        if (*p == '?') {
            pat.push_back(-1);
            while (*p == '?') ++p;
        } else {
            pat.push_back(static_cast<int>(std::strtoul(std::string(p, 2).c_str(), nullptr, 16)));
            p += 2;
        }
    }
    if (pat.empty() || m.image.size() < pat.size()) return -1;
    const auto* d = m.image.data();
    for (std::size_t i = 0; i + pat.size() <= m.image.size(); ++i) {
        std::size_t j = 0;
        while (j < pat.size() && (pat[j] < 0 || d[i + j] == pat[j])) ++j;
        if (j == pat.size()) return static_cast<std::ptrdiff_t>(i);
    }
    return -1;
}

// Match, then resolve a rip-relative rel32 located `rel_at` bytes into the match.
// Returns the RVA of the target, or 0.
inline std::uintptr_t scan_rip(const Module& m, const char* pattern, std::size_t rel_at) {
    std::ptrdiff_t pos = scan(m, pattern);
    if (pos < 0) return 0;
    std::int32_t rel;
    std::memcpy(&rel, m.image.data() + pos + rel_at, sizeof(rel));
    return static_cast<std::uintptr_t>(static_cast<std::int64_t>(pos + rel_at + 4) + rel);
}

// Match, then read a u32 immediate located `imm_at` bytes into the match.
inline bool scan_u32(const Module& m, const char* pattern, std::size_t imm_at, std::uint32_t& out) {
    std::ptrdiff_t pos = scan(m, pattern);
    if (pos < 0) return false;
    std::memcpy(&out, m.image.data() + pos + imm_at, sizeof(out));
    return true;
}

// SchemaSystem walk (layouts taken from src/source2/schema_system/*.rs)

// Collects the data pointers stored in a CUtlTSHash<..., 256 buckets> located at `hash_addr`.
inline std::vector<std::uintptr_t> ts_hash_elements(std::uintptr_t hash_addr) {
    constexpr std::size_t kBuckets = 256, kBucketsOff = 0x60, kBucketSize = 0x18, kTotal = 0x1870;
    std::vector<std::uint8_t> h(kTotal);
    std::vector<std::uintptr_t> out;
    if (!read_buf(hash_addr, h.data(), h.size())) return out;

    auto i32 = [&](std::size_t o) {
        std::int32_t v;
        std::memcpy(&v, &h[o], 4);
        return v;
    };
    auto u64 = [&](std::size_t o) {
        std::uintptr_t v;
        std::memcpy(&v, &h[o], 8);
        return v;
    };

    std::unordered_set<std::uintptr_t> seen;
    auto push = [&](std::uintptr_t p) {
        if (p && seen.insert(p).second) out.push_back(p);
    };

    // committed elements: walk each bucket's chain
    const std::size_t used = static_cast<std::size_t>(i32(0x0C));
    std::size_t count = 0;
    for (std::size_t b = 0; b < kBuckets && count < used; ++b) {
        std::uintptr_t node = u64(kBucketsOff + b * kBucketSize + 0x10);  // first_uncommitted
        for (int guard = 0; node && guard < 100000; ++guard) {
            std::uintptr_t next = 0, data = 0;
            if (!read_raw(node + 0x8, next) || !read_raw(node + 0x10, data)) break;
            if (data) {
                push(data);
                ++count;
            }
            if (count >= used) break;
            node = next;
        }
    }

    // free-list blobs
    const std::size_t peak = static_cast<std::size_t>(i32(0x10));
    std::uintptr_t blob = u64(0x20);
    std::size_t n = 0;
    for (int guard = 0; blob && guard < 100000; ++guard) {
        std::uintptr_t next = 0, data = 0;
        if (!read_raw(blob + 0x0, next) || !read_raw(blob + 0x10, data)) break;
        if (data) {
            push(data);
            ++n;
        }
        if (n >= peak) break;
        blob = next;
    }
    return out;
}

struct FieldRequest {
    const char* class_name;
    const char* field_name;
    std::ptrdiff_t* target;
    bool found = false;
};

inline bool resolve_netvars(const Module& schemasystem, FieldRequest* reqs, std::size_t req_count) {
    std::uintptr_t rva = scan_rip(schemasystem, "4C 8D 35 ? ? ? ? 0F 28 45", 3);
    if (!rva) return fail("schemasystem pattern is outdated");
    const std::uintptr_t sys = schemasystem.base + rva;

    std::int32_t scope_count = 0, registrations = 0;
    std::uintptr_t scopes = 0;
    if (!read_raw(sys + 0x190, scope_count) || !read_raw(sys + 0x198, scopes) ||
        !read_raw(sys + 0x280, registrations))
        return fail("failed to read SchemaSystem");
    if (registrations == 0) return fail("schema system has no registrations yet (game still loading?)");

    for (std::int32_t i = 0; i < scope_count; ++i) {
        std::uintptr_t scope = 0;
        if (!read_raw(scopes + static_cast<std::uintptr_t>(i) * 8, scope)) continue;
        if (read_cstr(scope + 0x8) != "client.dll") continue;

        for (std::uintptr_t cls : ts_hash_elements(scope + 0x560)) {
            std::uintptr_t name_ptr = 0;
            if (!read_raw(cls + 0x8, name_ptr)) continue;
            std::string cname = read_cstr(name_ptr, 128);

            for (std::size_t r = 0; r < req_count; ++r) {
                if (cname != reqs[r].class_name) continue;

                std::int16_t field_count = 0;
                std::uintptr_t fields = 0;
                if (!read_raw(cls + 0x24, field_count) || !read_raw(cls + 0x30, fields)) continue;

                for (std::int16_t f = 0; f < field_count; ++f) {
                    std::uintptr_t fptr = fields + static_cast<std::uintptr_t>(f) * 0x20;
                    std::uintptr_t fname = 0;
                    std::int32_t off = 0;
                    if (!read_raw(fptr, fname) || !read_raw(fptr + 0x10, off)) continue;
                    if (read_cstr(fname, 128) == reqs[r].field_name) {
                        *reqs[r].target = off;
                        reqs[r].found = true;
                        break;
                    }
                }
            }
        }
        break;
    }

    for (std::size_t r = 0; r < req_count; ++r)
        if (!reqs[r].found)
            return fail(std::string("netvar not found: ") + reqs[r].class_name +
                        "::" + reqs[r].field_name);
    return true;
}

}  // namespace detail

// public API

inline const std::string& last_error() { return detail::error(); }

template <typename T>
inline T read(std::uintptr_t addr) {
    T v{};
    detail::read_raw(addr, v);
    return v;
}

inline void shutdown() {
    if (process) CloseHandle(process);
    process = nullptr;
    pid = 0;
    client_base = 0;
}

// Attaches to cs2.exe and fills all offsets. Safe to call again (e.g. after a game restart).
inline bool init() {
    using namespace detail;
    shutdown();

    pid = find_pid(L"cs2.exe");
    if (!pid) return fail("cs2.exe not found");

    process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process) return fail("OpenProcess failed (run as administrator?)");

    Module client, schema;
    if (!load_module(L"client.dll", client)) return fail("client.dll not loaded");
    if (!load_module(L"schemasystem.dll", schema)) return fail("schemasystem.dll not loaded");
    client_base = client.base;

    // dwEntityList: mov [rip+x], rcx ; jmp ; int3
    dwEntityList = scan_rip(client, "48 89 0D ? ? ? ? E9 ? ? ? ? CC", 3);
    if (!dwEntityList) return fail("dwEntityList pattern is outdated");

    // dwLocalPlayerPawn: offset inside the CPrediction global (dwPrediction + imm32)
    std::uintptr_t prediction =
        scan_rip(client, "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54", 3);
    std::uint32_t pawn_in_prediction = 0;
    if (!prediction || !scan_u32(client, "4C 39 B6 ? ? ? ? 74 ? 44 88 BE", 3, pawn_in_prediction))
        return fail("dwLocalPlayerPawn pattern is outdated");
    dwLocalPlayerPawn = prediction + pawn_in_prediction;

    FieldRequest reqs[] = {
        {"CCSPlayerController", "m_hPlayerPawn", &m_hPlayerPawn},
        {"C_BaseEntity", "m_iHealth", &m_iHealth},
        {"C_BaseEntity", "m_iTeamNum", &m_iTeamNum},
        {"C_CSPlayerPawn", "m_entitySpottedState", &m_entitySpottedState},
        {"EntitySpottedState_t", "m_bSpotted", &m_bSpotted},
    };
    return resolve_netvars(schema, reqs, sizeof(reqs) / sizeof(reqs[0]));
}

}  // namespace offsets
