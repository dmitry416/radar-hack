#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct ModuleInfo {
    std::uintptr_t base = 0;
    std::size_t size = 0;
};

class Process {
public:
    Process() = default;
    ~Process();
    Process(const Process &) = delete;
    Process &operator=(const Process &) = delete;

    bool attach(const char *processName);
    void detach();

    bool isAlive() const;
    bool isForeground() const;

    DWORD id() const { return processId; }
    const std::string &lastError() const { return error; }

    bool findModule(const char *name, ModuleInfo &out) const;
    bool readBuffer(std::uintptr_t address, void *out, std::size_t size) const;
    std::string readString(std::uintptr_t address, std::size_t maxLength) const;

    template<typename T>
    std::optional<T> read(std::uintptr_t address) const {
        T value{};
        if (!readBuffer(address, &value, sizeof(T))) return std::nullopt;
        return value;
    }

    template<typename T>
    bool write(std::uintptr_t address, const T &value) const {
        SIZE_T written = 0;
        return WriteProcessMemory(handle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), &written)
               && written == sizeof(T);
    }

private:
    HANDLE handle = nullptr;
    DWORD processId = 0;
    std::string error;
};
