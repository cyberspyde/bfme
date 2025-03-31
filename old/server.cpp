#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <thread>
#include <future>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <map>
#include <Windows.h>
#include <TlHelp32.h>
#pragma comment(lib, "ws2_32.lib")

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

// Client information
struct ClientInfo {
    SOCKET socket;
    std::vector<DWORD_PTR> addresses;
};

// Server functions
void StartServer(int port);
void HandleClient(SOCKET clientSocket, std::map<SOCKET, ClientInfo>& clients);
void BroadcastCommand(std::map<SOCKET, ClientInfo>& clients, Command cmd, DWORD value);
void ReceiveClientResults(std::map<SOCKET, ClientInfo>& clients);

int main() {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cout << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    // Start server on port 8888
    StartServer(8888);

    WSACleanup();
    return 0;
}

void StartServer(int port) {
    // Create socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cout << "Socket creation failed: " << WSAGetLastError() << std::endl;
        return;
    }

    // Bind socket
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        return;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        return;
    }

    std::cout << "Server started on port " << port << std::endl;

    // Client management
    std::map<SOCKET, ClientInfo> clients;
    std::vector<std::thread> clientThreads;
    std::map<std::string, SOCKET> clientAddresses; // Track unique client IPs

    // Accept client connections in a separate thread
    std::thread acceptThread([&]() {
        while (true) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);
            if (clientSocket == INVALID_SOCKET) {
                std::cout << "Accept failed: " << WSAGetLastError() << std::endl;
                continue;
            }

            // Get client IP address
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
            std::string clientIPStr(clientIP);
            
            // Check if this client is already connected
            if (clientAddresses.find(clientIPStr) != clientAddresses.end()) {
                // Client already connected, close the new socket
                std::cout << "Client at " << clientIPStr << " already connected, rejecting duplicate connection" << std::endl;
                closesocket(clientSocket);
                continue;
            }

            std::cout << "Client connected from " << clientIPStr << std::endl;
            
            // Add client to our maps
            clients[clientSocket] = {clientSocket, {}};
            clientAddresses[clientIPStr] = clientSocket;
            
            // Handle client in a new thread
            clientThreads.push_back(std::thread([clientSocket, clientIPStr, &clients, &clientAddresses]() {
                HandleClient(clientSocket, std::ref(clients));
                
                // When client disconnects, remove from the address map
                clientAddresses.erase(clientIPStr);
            }));
        }
    });

    // Get process name from user
    std::string processName = "lotrbfme.exe"; // Default process name
    std::cout << "Enter process name (default: " << processName << "): ";
    std::string input;
    std::getline(std::cin, input);
    if (!input.empty()) {
        processName = input;
    }
    
    // Get process ID
    DWORD processId = GetProcessIdByName(processName);
    if (processId == 0) {
        std::cout << "Process not found!" << std::endl;
        return;
    }
    
    std::cout << "Process found. ID: " << processId << std::endl;
    
    // Open process with necessary access rights
    HANDLE processHandle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, processId);
    if (processHandle == NULL) {
        std::cout << "Failed to open process. Error: " << GetLastError() << std::endl;
        return;
    }

    std::vector<DWORD_PTR> serverAddresses;
    
    // Server command loop
    while (true) {
        std::cout << "\nCommands: scan, next, modify, exit\n> ";
        std::string command;
        std::cin >> command;
        
        if (command == "scan") {
            // Get initial value to search for
            DWORD searchValue;
            std::cout << "Enter initial value to search for: ";
            std::cin >> searchValue;
            
            // Get memory regions to scan
            auto startTime = std::chrono::high_resolution_clock::now();
            std::cout << "Identifying memory regions..." << std::endl;
            
            std::vector<std::pair<DWORD_PTR, SIZE_T>> memoryRegions = GetMemoryRegions(processHandle);
            std::cout << "Found " << memoryRegions.size() << " memory regions to scan" << std::endl;
            
            // Broadcast scan command to clients
            BroadcastCommand(clients, CMD_SCAN, searchValue);
            
            // Perform initial scan locally
            std::cout << "Starting initial scan for value " << searchValue << "..." << std::endl;
            serverAddresses = ScanMemoryRegionsInitialParallel(processHandle, memoryRegions, searchValue);
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            std::cout << "Server found " << serverAddresses.size() << " addresses with value " << searchValue 
                      << " in " << duration / 1000.0 << " seconds" << std::endl;
            
            // Receive scan results from clients
            ReceiveClientResults(clients);
            
            // Print combined results
            int totalAddresses = serverAddresses.size();
            for (const auto& client : clients) {
                totalAddresses += client.second.addresses.size();
                std::cout << "Client on socket " << client.first << " found " 
                          << client.second.addresses.size() << " addresses" << std::endl;
            }
            
            std::cout << "Total addresses across all machines: " << totalAddresses << std::endl;
        }
        else if (command == "next") {
            if (serverAddresses.empty()) {
                std::cout << "No addresses to scan. Run an initial scan first." << std::endl;
                continue;
            }
            
            DWORD newValue;
            std::cout << "Enter the new value to search for: ";
            std::cin >> newValue;
            
            // Broadcast next scan command to clients
            BroadcastCommand(clients, CMD_NEXT_SCAN, newValue);
            
            // Perform next scan locally
            auto startTime = std::chrono::high_resolution_clock::now();
            serverAddresses = NextScan(processHandle, serverAddresses, newValue);
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            std::cout << "Server found " << serverAddresses.size() << " addresses with value " << newValue 
                      << " in " << duration / 1000.0 << " seconds" << std::endl;
            
            // Receive scan results from clients
            ReceiveClientResults(clients);
            
            // Print combined results
            int totalAddresses = serverAddresses.size();
            for (const auto& client : clients) {
                totalAddresses += client.second.addresses.size();
                std::cout << "Client on socket " << client.first << " found " 
                          << client.second.addresses.size() << " addresses" << std::endl;
            }
            
            std::cout << "Total addresses across all machines: " << totalAddresses << std::endl;
            
            // Check if we've reached target number of addresses
            if (serverAddresses.size() < 5) {
                bool clientsReady = true;
                for (const auto& client : clients) {
                    if (client.second.addresses.size() != 1) {
                        clientsReady = false;
                        break;
                    }
                }
                
                if (clientsReady && !clients.empty()) {
                    std::cout << "Target reached! Server has " << serverAddresses.size() 
                              << " addresses and each client has 1 address." << std::endl;
                    std::cout << "Ready for modification." << std::endl;
                }
            }
        }
        else if (command == "modify") {
            if (serverAddresses.empty()) {
                std::cout << "No addresses to modify. Run scans first." << std::endl;
                continue;
            }
            
            // Check if clients have exactly 1 address each
            bool clientsReady = true;
            for (const auto& client : clients) {
                if (client.second.addresses.size() != 1) {
                    clientsReady = false;
                    std::cout << "Client on socket " << client.first << " has " 
                              << client.second.addresses.size() << " addresses, needs exactly 1." << std::endl;
                }
            }
            
            if (!clientsReady) {
                std::cout << "Not all clients have exactly 1 address. Run more scans." << std::endl;
                continue;
            }
            
            // Display server addresses
            std::cout << "Server addresses:" << std::endl;
            for (const auto& addr : serverAddresses) {
                DWORD currentValue;
                ReadProcessMemory(processHandle, (LPCVOID)addr, &currentValue, sizeof(currentValue), NULL);
                std::cout << "Address: 0x" << std::hex << addr << " - Value: " << std::dec << currentValue << std::endl;
            }
            
            // Get value to write
            DWORD newValue;
            std::cout << "Enter the value you want to write to memory: ";
            std::cin >> newValue;
            
            // Broadcast modify command to clients
            BroadcastCommand(clients, CMD_MODIFY, newValue);
            
            // Modify server addresses
            for (const auto& addr : serverAddresses) {
                WriteProcessMemory(processHandle, (LPVOID)addr, &newValue, sizeof(newValue), NULL);
            }
            
            std::cout << "Modified " << serverAddresses.size() << " server addresses and 1 address on each client to value " << newValue << std::endl;
        }
        else if (command == "exit") {
            // Broadcast exit command to clients
            BroadcastCommand(clients, CMD_EXIT, 0);
            break;
        }
        else {
            std::cout << "Unknown command" << std::endl;
        }
    }
    
    // Cleanup
    CloseHandle(processHandle);
    closesocket(serverSocket);
    
    // Wait for all client threads to finish
    acceptThread.detach();
    for (auto& thread : clientThreads) {
        thread.detach();
    }
}

void HandleClient(SOCKET clientSocket, std::map<SOCKET, ClientInfo>& clients) {
    // Client handler - this mostly waits for results from clients
    while (true) {
        // Check if client disconnected
        char buffer[4];
        int result = recv(clientSocket, buffer, sizeof(buffer), MSG_PEEK);
        if (result == 0 || result == SOCKET_ERROR) {
            std::cout << "Client disconnected" << std::endl;
            clients.erase(clientSocket);
            closesocket(clientSocket);
            return;
        }
        
        // Sleep to avoid high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void BroadcastCommand(std::map<SOCKET, ClientInfo>& clients, Command cmd, DWORD value) {
    // Prepare command packet
    char buffer[8];
    *(Command*)buffer = cmd;
    *(DWORD*)(buffer + 4) = value;
    
    // Send to all clients
    for (auto it = clients.begin(); it != clients.end();) {
        int result = send(it->first, buffer, sizeof(buffer), 0);
        if (result == SOCKET_ERROR) {
            std::cout << "Failed to send command to client. Error: " << WSAGetLastError() << std::endl;
            closesocket(it->first);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void ReceiveClientResults(std::map<SOCKET, ClientInfo>& clients) {
    // Give clients time to process
    std::cout << "Waiting for client results..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Receive results from each client
    for (auto it = clients.begin(); it != clients.end();) {
        // First receive number of addresses
        int numAddresses;
        int result = recv(it->first, (char*)&numAddresses, sizeof(numAddresses), 0);
        
        if (result <= 0) {
            std::cout << "Failed to receive from client. Error: " << WSAGetLastError() << std::endl;
            closesocket(it->first);
            it = clients.erase(it);
            continue;
        }
        
        // Clear previous addresses
        it->second.addresses.clear();
        
        // Receive each address
        for (int i = 0; i < numAddresses; i++) {
            DWORD_PTR address;
            result = recv(it->first, (char*)&address, sizeof(address), 0);
            
            if (result <= 0) {
                std::cout << "Failed to receive address from client. Error: " << WSAGetLastError() << std::endl;
                break;
            }
            
            it->second.addresses.push_back(address);
        }
        
        ++it;
    }
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