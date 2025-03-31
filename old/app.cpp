// app.cpp
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <fstream>

// Utility: Get process ID by name
DWORD GetProcessID(const std::wstring& processName) {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                DWORD pid = entry.th32ProcessID;
                CloseHandle(snapshot);
                return pid;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}

// DLL Injection
bool InjectDLL(DWORD pid, const std::string& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return false;

    LPVOID allocMem = VirtualAllocEx(hProcess, nullptr, dllPath.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProcess, allocMem, dllPath.c_str(), dllPath.size(), nullptr);

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                        (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32, "LoadLibraryA"),
                                        allocMem, 0, nullptr);
    if (hThread) CloseHandle(hThread);

    CloseHandle(hProcess);
    return true;
}

// Auto Pattern Scanner
uintptr_t PatternScan(HANDLE hProcess, uintptr_t start, size_t size, const char* pattern, const char* mask) {
    std::vector<BYTE> buffer(size);
    SIZE_T bytesRead;
    if (!ReadProcessMemory(hProcess, (LPCVOID)start, buffer.data(), size, &bytesRead))
        return 0;

    for (size_t i = 0; i < bytesRead; i++) {
        bool found = true;
        for (size_t j = 0; j < strlen(mask); j++) {
            if (mask[j] != '?' && pattern[j] != buffer[i + j]) {
                found = false;
                break;
            }
        }
        if (found) return start + i;
    }
    return 0;
}

// Pointer Resolver
uintptr_t ResolvePointer(HANDLE hProcess, uintptr_t base, std::vector<unsigned int> offsets) {
    uintptr_t addr = base;
    for (unsigned int offset : offsets) {
        ReadProcessMemory(hProcess, (LPCVOID)addr, &addr, sizeof(addr), 0);
        addr += offset;
    }
    return addr;
}

// Example Memory Modification Function
bool WriteToMemory(HANDLE hProcess, uintptr_t address, int value) {
    return WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(value), nullptr);
}

int main() {
    std::wstring targetProcess = L"lotrbfme.exe"; // Target process name
    std::string dllPath = "C:\\path_to\\your_cheat.dll"; // DLL to inject

    std::cout << "Waiting for process...\n";
    DWORD pid = 0;
    while (!(pid = GetProcessID(targetProcess))) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Process found! PID: " << pid << "\n";

    if (InjectDLL(pid, dllPath)) {
        std::cout << "DLL injected successfully!\n";
    } else {
        std::cout << "DLL injection failed!\n";
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open target process.\n";
        return -1;
    }

    // Example: Write value 9999 to a known address (replace address with your target)
    uintptr_t address = 0x12345678;
    int newValue = 9999;

    if (WriteToMemory(hProcess, address, newValue)) {
        std::cout << "Memory written successfully!\n";
    } else {
        std::cout << "Failed to write memory.\n";
    }

    CloseHandle(hProcess);
    return 0;
}