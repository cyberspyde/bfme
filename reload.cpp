#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")  // Link the necessary library


// Define the original function pointer type
typedef void (*PostStaticInitFunc)();

// Store the original function pointer
PostStaticInitFunc OriginalPostStaticInit = nullptr;

// Global handle to the game process
HANDLE gameProcess = NULL;


std::wstring ConvertToWString(const char* str) {
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    std::wstring wstr(len - 1, L'\0');  // Adjust length to exclude null terminator
    MultiByteToWideChar(CP_ACP, 0, str, -1, &wstr[0], len - 1);
    return wstr;
}

DWORD GetProcessIDByName(const std::wstring& processName) {
    DWORD processID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnapshot, &pe)) {
            do {
                if (std::wstring(pe.szExeFile, pe.szExeFile + strlen(pe.szExeFile)) == processName) {  // Convert CHAR array to std::wstring
                    processID = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return processID;
}

// Thread function to monitor hotkeys
DWORD WINAPI HotkeyThread(LPVOID lpParam) {
    std::cout << "Hotkey monitor started. Press F9 to reload configuration." << std::endl;
    while (true) {
        if (GetAsyncKeyState(VK_F9) & 0x8000) {
            std::cout << "F9 pressed - calling PostStaticInit..." << std::endl;
            
            HANDLE remoteThread = CreateRemoteThread(
                gameProcess,
                NULL, 
                0, 
                (LPTHREAD_START_ROUTINE)OriginalPostStaticInit,
                NULL, 
                0, 
                NULL
            );
            
            if (remoteThread) {
                std::cout << "Function called successfully!" << std::endl;
                CloseHandle(remoteThread);
            } else {
                std::cout << "Failed to call function. Error: " << GetLastError() << std::endl;
            }
            Sleep(1000);
        }
        Sleep(100);
    }
    return 0;
}

int main() {
    std::cout << "BFME Configuration Reloader" << std::endl;
    std::cout << "-----------------------------" << std::endl;
    
    DWORD processID = GetProcessIDByName(L"lotrbfme.exe");
    if (processID == 0) {
        std::cout << "Game process not found. Is the game running?" << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    
    gameProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!gameProcess) {
        std::cout << "Could not open game process. Error: " << GetLastError() << std::endl;
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    
    HMODULE gameModule = NULL;
    DWORD cbNeeded;
    if (!EnumProcessModules(gameProcess, &gameModule, sizeof(gameModule), &cbNeeded)) {
        std::cout << "Could not enumerate process modules. Error: " << GetLastError() << std::endl;
        CloseHandle(gameProcess);
        std::cout << "Press any key to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
    
    uintptr_t functionAddress = (uintptr_t)gameModule + 0xdd2592;
    OriginalPostStaticInit = (PostStaticInitFunc)functionAddress;
    
    std::cout << "Game found! Process ID: " << processID << std::endl;
    std::cout << "Function address: 0x" << std::hex << functionAddress << std::dec << std::endl;
    
    CreateThread(NULL, 0, HotkeyThread, NULL, 0, NULL);
    
    std::cout << "Reloader is now active. Press Ctrl+C to exit." << std::endl;
    while (true) {
        Sleep(1000);
    }
    
    CloseHandle(gameProcess);
    return 0;
}
