#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <thread>
#include <future>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <TlHelp32.h>
#pragma comment(lib, "Ws2_32.lib")

// Function declarations from your original code
DWORD GetProcessIdByName(const std::string& processName);
std::vector<std::pair<DWORD_PTR, SIZE_T>> GetMemoryRegions(HANDLE processHandle);
std::vector<DWORD_PTR> ScanMemoryRegionsInitialParallel(HANDLE processHandle, const std::vector<std::pair<DWORD_PTR, SIZE_T>>& regions, DWORD valueToFind);
std::vector<DWORD_PTR> ScanMemoryRegion(HANDLE processHandle, DWORD_PTR baseAddress, SIZE_T regionSize, DWORD valueToFind);
std::vector<DWORD_PTR> NextScan(HANDLE processHandle, const std::vector<DWORD_PTR>& addresses, DWORD newValue);

// Network protocol commands
enum Command {
    CMD_SCAN = 1,
    CMD_NEXT_SCAN = 2,
    CMD_MODIFY = 3,
    CMD_EXIT = 4
};

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // Get server address from user
    std::string serverAddress = "26.1.238.127"; // Default address
    int serverPort = 8888;
    
    std::string processName = "lotrbfme.exe"; // Target process name
    DWORD processId = 0;
    HANDLE processHandle = NULL;
    
    // Variables to track our current state
    std::vector<DWORD_PTR> foundAddresses;
    bool shouldReconnect = true;
    
    while (true) {
        // Check if we need to find the process
        if (processHandle == NULL) {
            std::cout << "Waiting for process '" << processName << "' to start..." << std::endl;
            
            // Loop until we find the process
            while (processHandle == NULL) {
                processId = GetProcessIdByName(processName);
                
                if (processId != 0) {
                    // Process found, try to open it
                    processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, processId);
                    
                    if (processHandle != NULL) {
                        std::cout << "Process found and opened. ID: " << processId << std::endl;
                    } else {
                        std::cout << "Found process but failed to open. Error: " << GetLastError() << std::endl;
                        // Reset processId since we couldn't open it
                        processId = 0;
                    }
                }
                
                if (processHandle == NULL) {
                    // Wait before checking again
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        }
        
        // Now we have a valid process handle, try to connect to server
        SOCKET clientSocket = INVALID_SOCKET;
        bool connected = false;
        
        // Connection loop
        while (!connected) {
            // Create socket
            clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (clientSocket == INVALID_SOCKET) {
                std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            
            // Setup server address
            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(serverPort);
            inet_pton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr);
            
            // Try to connect
            std::cout << "Attempting to connect to server at " << serverAddress << ":" << serverPort << "..." << std::endl;
            result = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
            
            if (result == SOCKET_ERROR) {
                std::cout << "Connection failed: " << WSAGetLastError() << std::endl;
                closesocket(clientSocket);
                std::cout << "Retrying in 5 seconds..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
                // Check if the process is still running
                DWORD exitCode;
                if (!GetExitCodeProcess(processHandle, &exitCode) || exitCode != STILL_ACTIVE) {
                    std::cout << "Process has terminated. Waiting for it to restart..." << std::endl;
                    CloseHandle(processHandle);
                    processHandle = NULL;
                    break; // Break out of connection loop to find process again
                }
                
                continue;
            }
            
            connected = true;
            std::cout << "Connected to server!" << std::endl;
        }
        
        // If we lost the process, go back to the beginning to find it again
        if (processHandle == NULL) {
            continue;
        }
        
        // Main client loop - run while connected
        while (connected) {
            // First check if process is still running
            DWORD exitCode;
            if (!GetExitCodeProcess(processHandle, &exitCode) || exitCode != STILL_ACTIVE) {
                std::cout << "Process has terminated. Waiting for it to restart..." << std::endl;
                CloseHandle(processHandle);
                processHandle = NULL;
                connected = false;
                break;
            }
            
            // Wait for command from server
            char buffer[8];
            result = recv(clientSocket, buffer, sizeof(buffer), 0);
            
            if (result <= 0) {
                if (result == 0) {
                    std::cout << "Server closed connection" << std::endl;
                } else {
                    std::cout << "Connection error: " << WSAGetLastError() << std::endl;
                }
                closesocket(clientSocket);
                connected = false;
                break;
            }
            
            Command cmd = *(Command*)buffer;
            DWORD value = *(DWORD*)(buffer + 4);
            
            switch (cmd) {
                case CMD_SCAN: {
                    std::cout << "Starting initial scan for value " << value << "..." << std::endl;
                    
                    // Get memory regions
                    auto startTime = std::chrono::high_resolution_clock::now();
                    std::vector<std::pair<DWORD_PTR, SIZE_T>> memoryRegions = GetMemoryRegions(processHandle);
                    std::cout << "Found " << memoryRegions.size() << " memory regions to scan" << std::endl;
                    
                    // Perform scan
                    foundAddresses = ScanMemoryRegionsInitialParallel(processHandle, memoryRegions, value);
                    
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                    
                    std::cout << "Found " << foundAddresses.size() << " addresses with value " << value 
                              << " in " << duration / 1000.0 << " seconds" << std::endl;
                    
                    // Send results to server
                    int numAddresses = foundAddresses.size();
                    if (send(clientSocket, (char*)&numAddresses, sizeof(numAddresses), 0) == SOCKET_ERROR) {
                        std::cout << "Failed to send scan results" << std::endl;
                        connected = false;
                        break;
                    }
                    
                    for (const auto& addr : foundAddresses) {
                        if (send(clientSocket, (char*)&addr, sizeof(addr), 0) == SOCKET_ERROR) {
                            std::cout << "Failed to send address data" << std::endl;
                            connected = false;
                            break;
                        }
                    }
                    
                    break;
                }
                
                // Other cases remain the same...
                case CMD_NEXT_SCAN: {
                    // ... (same implementation as before)
                    if (foundAddresses.empty()) {
                        std::cout << "No addresses to scan" << std::endl;
                        int numAddresses = 0;
                        if (send(clientSocket, (char*)&numAddresses, sizeof(numAddresses), 0) == SOCKET_ERROR) {
                            std::cout << "Failed to send scan results" << std::endl;
                            connected = false;
                            break;
                        }
                        break;
                    }
                    
                    std::cout << "Performing next scan for value " << value << "..." << std::endl;
                    
                    // Perform next scan
                    auto startTime = std::chrono::high_resolution_clock::now();
                    foundAddresses = NextScan(processHandle, foundAddresses, value);
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                    
                    std::cout << "Found " << foundAddresses.size() << " addresses with new value " << value 
                              << " in " << duration / 1000.0 << " seconds" << std::endl;
                    
                    // Send results to server
                    int numAddresses = foundAddresses.size();
                    if (send(clientSocket, (char*)&numAddresses, sizeof(numAddresses), 0) == SOCKET_ERROR) {
                        std::cout << "Failed to send scan results" << std::endl;
                        connected = false;
                        break;
                    }
                    
                    for (const auto& addr : foundAddresses) {
                        if (send(clientSocket, (char*)&addr, sizeof(addr), 0) == SOCKET_ERROR) {
                            std::cout << "Failed to send address data" << std::endl;
                            connected = false;
                            break;
                        }
                    }
                    
                    break;
                }
                
                case CMD_MODIFY: {
                    if (foundAddresses.size() != 1) {
                        std::cout << "Expected exactly 1 address to modify, but have " << foundAddresses.size() << std::endl;
                        break;
                    }
                    
                    std::cout << "Modifying address to value " << value << "..." << std::endl;
                    
                    // Modify the address
                    DWORD_PTR addr = foundAddresses[0];
                    DWORD currentValue;
                    ReadProcessMemory(processHandle, (LPCVOID)addr, &currentValue, sizeof(currentValue), NULL);
                    
                    std::cout << "Modifying address 0x" << std::hex << addr << " from " << std::dec 
                              << currentValue << " to " << value << std::endl;
                    
                    WriteProcessMemory(processHandle, (LPVOID)addr, &value, sizeof(value), NULL);
                    
                    std::cout << "Memory modified successfully" << std::endl;
                    break;
                }
                
                case CMD_EXIT: {
                    std::cout << "Server requested exit" << std::endl;
                    if (processHandle != NULL) {
                        CloseHandle(processHandle);
                    }
                    closesocket(clientSocket);
                    WSACleanup();
                    return 0;
                }
                
                default:
                    std::cout << "Unknown command: " << cmd << std::endl;
                    break;
            }
        }
        
        if (connected) {
            closesocket(clientSocket);
        }
        
        std::cout << "Connection lost. Will reconnect when ready..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // Cleanup (though we should never reach here)
    if (processHandle != NULL) {
        CloseHandle(processHandle);
    }
    WSACleanup();
    return 0;
}

// Include your original functions here
// GetProcessIdByName, GetMemoryRegions, ScanMemoryRegionsInitialParallel, ScanMemoryRegion, NextScan
// From your original code - add these at the end of both server and client files
DWORD GetProcessIdByName(const std::string& processName) {
    DWORD processId = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);
        
        if (Process32First(snapshot, &processEntry)) {
            do {
                if (_stricmp(processEntry.szExeFile, processName.c_str()) == 0) {
                    processId = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    
    return processId;
}

std::vector<std::pair<DWORD_PTR, SIZE_T>> GetMemoryRegions(HANDLE processHandle) {
    std::vector<std::pair<DWORD_PTR, SIZE_T>> regions;
    
    MEMORY_BASIC_INFORMATION memInfo;
    DWORD_PTR address = 0;
    
    while (VirtualQueryEx(processHandle, (LPCVOID)address, &memInfo, sizeof(memInfo))) {
        // Only consider committed memory that is readable and writable
        if (memInfo.State == MEM_COMMIT && 
            (memInfo.Protect & PAGE_READWRITE) && 
            !(memInfo.Protect & PAGE_GUARD)) {
            
            regions.push_back(std::make_pair((DWORD_PTR)memInfo.BaseAddress, memInfo.RegionSize));
        }
        
        address = (DWORD_PTR)memInfo.BaseAddress + memInfo.RegionSize;
        
        // Break if we've reached too far
        if (address > 0x7FFFFFFF) break;
    }
    
    return regions;
}

std::vector<DWORD_PTR> ScanMemoryRegion(HANDLE processHandle, DWORD_PTR baseAddress, SIZE_T regionSize, DWORD valueToFind) {
    std::vector<DWORD_PTR> foundAddresses;
    
    // Allocate buffer for reading memory
    std::vector<BYTE> buffer(regionSize);
    SIZE_T bytesRead;
    
    // Read memory region into buffer
    if (ReadProcessMemory(processHandle, (LPCVOID)baseAddress, buffer.data(), regionSize, &bytesRead)) {
        // Scan buffer for value (only at 4-byte aligned positions)
        for (SIZE_T offset = 0; offset <= bytesRead - sizeof(DWORD); offset += 4) {
            DWORD value = *(DWORD*)(&buffer[offset]);
            if (value == valueToFind) {
                foundAddresses.push_back(baseAddress + offset);
            }
        }
    }
    
    return foundAddresses;
}

std::vector<DWORD_PTR> ScanRegionTask(HANDLE processHandle, DWORD_PTR baseAddress, SIZE_T regionSize, DWORD valueToFind) {
    // Duplicate the process handle for this thread
    HANDLE threadHandle;
    DuplicateHandle(GetCurrentProcess(), processHandle, GetCurrentProcess(), &threadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    
    auto result = ScanMemoryRegion(threadHandle, baseAddress, regionSize, valueToFind);
    
    CloseHandle(threadHandle);
    return result;
}

std::vector<DWORD_PTR> ScanMemoryRegionsInitialParallel(HANDLE processHandle, const std::vector<std::pair<DWORD_PTR, SIZE_T>>& regions, DWORD valueToFind) {
    std::vector<DWORD_PTR> foundAddresses;
    
    // Get number of available cores (leave one for OS)
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // Default if detection fails
    numThreads = numThreads > 1 ? numThreads - 1 : 1;
    
    // Split regions into batches for parallel processing
    std::vector<std::future<std::vector<DWORD_PTR>>> futures;
    
    size_t regionCount = regions.size();
    size_t batchSize = (regionCount + numThreads - 1) / numThreads;
    
    std::cout << "Using " << numThreads << " threads for scanning" << std::endl;
    
    for (size_t threadIdx = 0; threadIdx < numThreads; threadIdx++) {
        size_t startIdx = threadIdx * batchSize;
        size_t endIdx = std::min(startIdx + batchSize, regionCount);
        
        if (startIdx >= regionCount) break;
        
        // Launch task for this batch
        futures.push_back(std::async(std::launch::async, [=]() {
            std::vector<DWORD_PTR> threadResults;
            
            for (size_t i = startIdx; i < endIdx; i++) {
                auto regionResults = ScanRegionTask(processHandle, regions[i].first, regions[i].second, valueToFind);
                threadResults.insert(threadResults.end(), regionResults.begin(), regionResults.end());
            }
            
            return threadResults;
        }));
    }
    
    // Collect results from all threads
    for (auto& future : futures) {
        auto threadResults = future.get();
        foundAddresses.insert(foundAddresses.end(), threadResults.begin(), threadResults.end());
    }
    
    return foundAddresses;
}

std::vector<DWORD_PTR> NextScan(HANDLE processHandle, const std::vector<DWORD_PTR>& addresses, DWORD newValue) {
    std::vector<DWORD_PTR> filteredAddresses;
    
    for (const auto& addr : addresses) {
        DWORD currentValue;
        if (ReadProcessMemory(processHandle, (LPCVOID)addr, &currentValue, sizeof(currentValue), NULL)) {
            if (currentValue == newValue) {
                filteredAddresses.push_back(addr);
            }
        }
    }
    
    return filteredAddresses;
}