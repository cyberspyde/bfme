#ifndef MEMORY_SCANNER_H
#define MEMORY_SCANNER_H

#include <windows.h>
#include <vector>
#include <string>

class MemoryScanner {
public:
    // Function to scan memory and return addresses with their values
    static std::vector<std::pair<DWORD_PTR, DWORD>> ScanMemory(const std::string& processName);
};

#endif // MEMORY_SCANNER_H