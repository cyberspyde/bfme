#include "modify.h"
#include <sstream>
#include <string>
#include <cstdlib>
#include <windows.h>
#include <TlHelp32.h>

// Helper function to get the process handle
HANDLE GetProcessHandle(const std::string& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    PROCESSENTRY32 processEntry = {};
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE processHandle = NULL;
    if (Process32First(snapshot, &processEntry)) {
        do {
            if (_stricmp(processEntry.szExeFile, processName.c_str()) == 0) {
                processHandle = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, processEntry.th32ProcessID);
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return processHandle;
}

// Function to handle the "Modify" button functionality
void ModifyValue(HWND hwnd, int selectedAddressIndex, std::vector<std::pair<DWORD_PTR, DWORD>>& guiResults, HWND hEditValue) {
    if (selectedAddressIndex >= 0 && selectedAddressIndex < guiResults.size()) {
        const auto& [address, currentValue] = guiResults[selectedAddressIndex];

        // Get the new value from the edit control
        char newValueStr[32] = {0};
        GetWindowTextA(hEditValue, newValueStr, sizeof(newValueStr));
        DWORD newValue = std::atoi(newValueStr);

        // Get the process handle
        HANDLE processHandle = GetProcessHandle("lotrbfme.exe");
        if (processHandle == NULL) {
            MessageBoxA(hwnd, "Failed to open process!", "Error", MB_OK | MB_ICONERROR);
            return;
        }

        // Write the new value to memory
        SIZE_T bytesWritten = 0;
        if (WriteProcessMemory(processHandle, reinterpret_cast<LPVOID>(address), &newValue, sizeof(newValue), &bytesWritten)) {
            // Update the displayed value
            guiResults[selectedAddressIndex].second = newValue;

            // Show success message
            std::ostringstream oss;
            oss << "Address: 0x" << std::hex << std::uppercase << address 
                << "\nOld Value: " << std::dec << currentValue
                << "\nNew Value: " << newValue
                << "\nBytes Written: " << bytesWritten;
            MessageBoxA(hwnd, oss.str().c_str(), "Value Modified", MB_OK | MB_ICONINFORMATION);
        } else {
            // Show error message
            MessageBoxA(hwnd, "Failed to write to memory!", "Error", MB_OK | MB_ICONERROR);
        }

        // Close the process handle
        CloseHandle(processHandle);
    } else {
        MessageBoxA(hwnd, "No address selected!", "Error", MB_OK | MB_ICONERROR);
    }
}