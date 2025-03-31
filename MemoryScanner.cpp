#include "MemoryScanner.h"
#include <iostream>
#include <TlHelp32.h>

DWORD GetProcessId(const char* processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &processEntry)) {
            do {
                if (_stricmp(processEntry.szExeFile, processName) == 0) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return processId;
}

DWORD_PTR ResolvePointerPath(HANDLE processHandle, DWORD_PTR baseAddress, const std::vector<DWORD>& offsets) {
    DWORD_PTR currentAddress = baseAddress;

    for (size_t i = 0; i < offsets.size(); ++i) {
        DWORD_PTR readAddress;
        SIZE_T bytesRead;
        if (!ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(currentAddress), &readAddress, sizeof(readAddress), &bytesRead)) {
            return 0;
        }
        currentAddress = readAddress + offsets[i];
    }
    return currentAddress;
}

std::vector<std::pair<DWORD_PTR, DWORD>> MemoryScanner::ScanMemory(const std::string& processName) {
    std::vector<std::pair<DWORD_PTR, DWORD>> results;

    DWORD processId = GetProcessId(processName.c_str());
    if (processId == 0) {
        std::cerr << "Process not found" << std::endl;
        return results;
    }

    HANDLE processHandle = OpenProcess(PROCESS_VM_READ, FALSE, processId);
    if (processHandle == NULL) {
        std::cerr << "Failed to open process" << std::endl;
        return results;
    }

    std::vector<std::pair<DWORD_PTR, std::vector<DWORD>>> pointerPaths = {
        {0x00EED748, {0x1C, 0x220, 0xC, 0x1DC, 0xF9C}},
        {0x00EED748, {0x1C, 0x694, 0x220, 0xC, 0x1DC, 0xF9C}},
        {0x00EED748, {0x1C, 0x220, 0xC, 0x1DC, 0xF9C}},
        {0x00F4C6C8, {0x0, 0x178, 0x1C, 0x220, 0xC, 0x1DC, 0xF9C}},
        {0x00EED748, {0x1C, 0xB0, 0x220, 0xC, 0x1DC, 0xF9C}},
        {0x00EED748, {0x1C, 0x220, 0xC, 0x694, 0x1DC, 0xF9C}},
        {0x00EED748, {0x18, 0x694, 0x290, 0xC, 0x6A0, 0x1DC, 0xF9C}},
        {0x00EED748, {0x18, 0x290, 0x10, 0xBC, 0x1DC, 0xF9C}},
        {0x00EED748, {0x18, 0xB0, 0x290, 0x10, 0x6A0, 0x1DC, 0xF9C}},
        {0x00EED748, {0x18, 0x290, 0xC, 0x6A0, 0x1E4, 0xC98}},
        {0x00EED748, {0x18, 0x694, 0xB0, 0x290, 0x10, 0x1EC, 0xC98}},
        {0x00F4C6C8, {0x0, 0x178, 0x1C, 0xB0, 0x1E4, 0xC98}},
        {0x00EED748, {0x1C, 0x694, 0xB0, 0x1E0, 0xC98}},
        {0x00EED748, {0x18, 0x290, 0xC, 0xBC, 0x1E0, 0xC98}},
        {0x00EEF190, {0x10, 0x0, 0x0, 0x8, 0x14, 0x1E0, 0xC98}},
        {0x00F4C6C8, {0x0, 0x178, 0x18, 0x290, 0x10, 0x1FC, 0xC8C}},
        {0x00EED748, {0x18, 0x290, 0x18, 0x18, 0x1FC, 0xC8C}},
        {0x00EED748, {0x1C, 0x694, 0x220, 0xC, 0x28C, 0x10, 0x368}},
        {0x00EF079C, {0x14, 0x28C, 0x1C, 0x4, 0x0, 0x18, 0x368}},
        {0x00EF079C, {0x14, 0x694, 0x28C, 0x1C, 0x18, 0x368}},
        {0x00EED748, {0x20, 0xB0, 0x694, 0x290, 0x1C, 0x1C, 0x58}},
        {0x00EED748, {0x1C, 0x290, 0x10, 0x58}},
        {0x00EED748, {0x1C, 0xB0, 0x220, 0xC, 0x290, 0xC, 0x58}},
        {0x00EED748, {0x20, 0x694, 0x28C, 0x1C, 0x0, 0x18, 0x58}}
    };

    DWORD_PTR moduleBase = 0x400000;
    DWORD_PTR finalFinalAddress = 0;
    bool allMatch = true;

    for (const auto& path : pointerPaths) {
        DWORD_PTR baseAddress = moduleBase + path.first;
        DWORD_PTR finalAddress = ResolvePointerPath(processHandle, baseAddress, path.second);

        if (finalAddress) {
            if (finalFinalAddress == 0) {
                finalFinalAddress = finalAddress;
            } else if (finalFinalAddress != finalAddress) {
                allMatch = false;
                break;
            }
        } else {
            allMatch = false;
            break;
        }
    }

    if (allMatch && finalFinalAddress != 0) {
        std::vector<DWORD_PTR> allAddresses;
        allAddresses.push_back(finalFinalAddress);

        for (int i = 1; i < 8; ++i) {
            DWORD_PTR nextAddress = finalFinalAddress + (i * 0x1600);
            allAddresses.push_back(nextAddress);
        }

        for (DWORD_PTR address : allAddresses) {
            DWORD value = 0;
            SIZE_T bytesRead = 0;

            if (ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address), &value, sizeof(value), &bytesRead)) {
                results.emplace_back(address, value);
            } else {
                results.emplace_back(address, 0); // Failed to read memory, set value to 0
            }
        }
    }

    CloseHandle(processHandle);
    return results;
}
