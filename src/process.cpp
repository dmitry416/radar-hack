#include "process.hpp"

#include <tlhelp32.h>
#include <cstring>

namespace {
    DWORD findProcessId(const char *name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        DWORD result = 0;
        if (Process32First(snap, &entry)) {
            do {
                if (_stricmp(entry.szExeFile, name) == 0) {
                    result = entry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snap, &entry));
        }

        CloseHandle(snap);
        return result;
    }
}

Process::~Process() {
    detach();
}

bool Process::attach(const char *processName) {
    detach();

    processId = findProcessId(processName);
    if (!processId) {
        error = std::string(processName) + " not found";
        return false;
    }

    handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                         PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!handle) {
        error = "failed to open process (try running as administrator)";
        processId = 0;
        return false;
    }
    return true;
}

void Process::detach() {
    if (handle) CloseHandle(handle);
    handle = nullptr;
    processId = 0;
}

bool Process::isAlive() const {
    DWORD code = 0;
    return handle && GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE;
}

bool Process::isForeground() const {
    const HWND window = GetForegroundWindow();
    if (!window) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    return pid == processId;
}

bool Process::findModule(const char *name, ModuleInfo &out) const {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snap == INVALID_HANDLE_VALUE) return false;

    MODULEENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Module32First(snap, &entry)) {
        do {
            if (_stricmp(entry.szModule, name) == 0) {
                out.base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
                out.size = entry.modBaseSize;
                found = true;
                break;
            }
        } while (Module32Next(snap, &entry));
    }

    CloseHandle(snap);
    return found;
}

bool Process::readBuffer(std::uintptr_t address, void *out, std::size_t size) const {
    SIZE_T read = 0;
    return ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), out, size, &read)
           && read == size;
}

std::string Process::readString(std::uintptr_t address, std::size_t maxLength) const {
    std::string text(maxLength, '\0');
    SIZE_T read = 0;
    ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), text.data(), maxLength, &read);
    text.resize(std::strlen(text.c_str()));
    return text;
}
