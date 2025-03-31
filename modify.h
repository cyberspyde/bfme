#ifndef MODIFY_H
#define MODIFY_H

#include <windows.h>
#include <string>
#include <vector>
#include <utility>

// Declare the GetProcessHandle function
HANDLE GetProcessHandle(const std::string& processName);

// Function to handle the "Modify" button functionality
void ModifyValue(HWND hwnd, int selectedAddressIndex, std::vector<std::pair<DWORD_PTR, DWORD>>& guiResults, HWND hEditValue);

#endif // MODIFY_H