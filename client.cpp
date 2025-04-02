#include <winsock2.h>
#include "client.h"
#include "modify.h"
#include <TlHelp32.h>
#include "MemoryScanner.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <windows.h> // Ensure DWORD and DWORD_PTR are defined
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

Client::Client(const std::string& serverAddress, int port)
    : serverAddress(serverAddress), port(port) {}

void Client::Connect() {
    WSADATA wsaData;
    struct sockaddr_in serverAddr;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << "\n";
        return;
    }

    // Create a socket
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Use the member variable
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return;
    }

    // Set up the server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    // Use InetPton instead of inet_pton
    if (InetPton(AF_INET, serverAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address or address not supported.\n";
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    // Connect to the server
    iResult = connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (iResult == SOCKET_ERROR) {
        std::cerr << "Connection to server failed: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    std::cout << "Connected to server at " << serverAddress << ":" << port << "\n";
}
void Client::Reconnect() {
    std::cout << "Reconnecting to server...\n";
    Connect();
    ScanMemory();
}

void Client::ScanMemory() {
    std::cout << "Scanning memory...\n";
    memoryAddresses = MemoryScanner::ScanMemory("lotrbfme.exe");
    DisplayAddresses();
}

void Client::SelectAddress(int index) {
    if (index < 0 || index >= memoryAddresses.size()) {
        std::cerr << "Invalid address index.\n";
        return;
    }
    std::cout << "Selected address: 0x" << std::hex << memoryAddresses[index].first << "\n";
    // Send selection command to the server
    std::ostringstream command;
    command << "SELECT " << index;
    // Send command to server (implement WebSocket send logic)
}

void Client::ModifyValue(int index, DWORD newValue) {
    if (index < 0 || index >= memoryAddresses.size()) {
        std::cerr << "Invalid address index.\n";
        return;
    }
    
    DWORD_PTR address = memoryAddresses[index].first;
    DWORD oldValue = memoryAddresses[index].second;
    
    std::cout << "Modifying address 0x" << std::hex << address
              << " from value " << std::dec << oldValue 
              << " to value " << newValue << "\n";
    
    // Get process handle
    HANDLE processHandle = GetProcessHandle("lotrbfme.exe");
    if (processHandle == NULL) {
        std::cerr << "Failed to open process!\n";
        return;
    }
    
    // Write the new value to memory
    SIZE_T bytesWritten = 0;
    if (WriteProcessMemory(processHandle, reinterpret_cast<LPVOID>(address), &newValue, sizeof(newValue), &bytesWritten)) {
        // Update our local copy of the value
        memoryAddresses[index].second = newValue;
        std::cout << "Memory successfully modified! Bytes written: " << bytesWritten << "\n";
    } else {
        std::cerr << "Failed to write to memory! Error: " << GetLastError() << "\n";
    }
    
    // Close the process handle
    CloseHandle(processHandle);
}

void Client::Run() {
    Connect();
    if (clientSocket == INVALID_SOCKET) {
        return; // Exit if connection failed
    }

    ScanMemory();

    char buffer[512];
    while (true) {
        // Wait for commands from the server
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string command(buffer);
            HandleServerCommands(command);
        } else if (bytesReceived == 0 || bytesReceived == SOCKET_ERROR) {
            std::cerr << "Disconnected from server.\n";
            break;
        }
    }

    // Cleanup
    closesocket(clientSocket);
    WSACleanup();
}

void Client::HandleServerCommands(const std::string& command) {
    if (command == "RECONNECT") {
        Reconnect();
    } else if (command.rfind("SELECT", 0) == 0) {
        int index = std::stoi(command.substr(7));
        SelectAddress(index);
    } else if (command.rfind("MODIFY", 0) == 0) {
        std::istringstream iss(command.substr(7));
        int index;
        DWORD newValue;
        iss >> index >> newValue;
        ModifyValue(index, newValue);
    }
}

void Client::DisplayAddresses() {
    std::cout << "Memory Addresses:\n";
    for (size_t i = 0; i < memoryAddresses.size(); ++i) {
        std::cout << i << ": 0x" << std::hex << memoryAddresses[i].first
                  << " -> Value: " << std::dec << memoryAddresses[i].second << "\n";
    }
}