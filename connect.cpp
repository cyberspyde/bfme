#include <winsock2.h>
#include "connect.h"
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <algorithm> // For std::remove
// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// Global list of connected client sockets
std::vector<SOCKET> clientSockets;
std::mutex clientSocketsMutex;

// Global variable to track the number of connected clients
std::atomic<int> connectedClientsCount(0);

void sendCommandToClients(const std::string& command);

// Server function to handle incoming connections
void StartServer() {
    WSADATA wsaData;
    SOCKET serverSocket = INVALID_SOCKET;
    struct addrinfo *result = NULL, hints;

    // Initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return;
    }

    // Set up the hints structure
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, "8080", &hints, &result);
    if (iResult != 0) {
        std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return;
    }

    // Create a socket for the server
    serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return;
    }

    // Bind the socket
    iResult = bind(serverSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    freeaddrinfo(result);

    // Listen for incoming connections
    iResult = listen(serverSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "Server is listening on port 8080..." << std::endl;

    // Accept incoming connections
    SOCKET clientSocket;
    // Modify the server loop to keep the connection open and handle clients in separate threads
    while (true) {
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            break;
        }
    
        // Increment the connected clients count
        connectedClientsCount++;
    
        std::cout << "Client connected! Total clients: " << connectedClientsCount.load() << std::endl;
    
        // Handle the client in a separate thread
        std::thread([clientSocket]() {
            {
                // Add the client socket to the list
                std::lock_guard<std::mutex> lock(clientSocketsMutex);
                clientSockets.push_back(clientSocket);
            }
        
            char buffer[512];
            const char *welcomeMessage = "Welcome to the server!";
            send(clientSocket, welcomeMessage, (int)strlen(welcomeMessage), 0);
        
            while (true) {
                int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                if (bytesReceived > 0) {
                    buffer[bytesReceived] = '\0';
                    std::cout << "Message from client: " << buffer << std::endl;
                } else if (bytesReceived == 0 || bytesReceived == SOCKET_ERROR) {
                    break; // Client disconnected or error occurred
                }
            }
        
            // Cleanup after client disconnects
            closesocket(clientSocket);
            connectedClientsCount--;
        
            {
                std::lock_guard<std::mutex> lock(clientSocketsMutex);
                clientSockets.erase(
                    std::remove(clientSockets.begin(), clientSockets.end(), static_cast<SOCKET>(clientSocket)),
                    clientSockets.end()
                );
            }
        
            std::cout << "Client disconnected! Total clients: " << connectedClientsCount.load() << std::endl;
        }).detach();
    }

    // Cleanup
    closesocket(serverSocket);
    WSACleanup();
}

// Function to handle the "Reconnect" button functionality
void issueReconnect() {
    // Start the server in a separate thread
    std::thread serverThread(StartServer);
    serverThread.detach(); // Detach the thread to run in the background

    sendCommandToClients("RECONNECT");

}

// Function to get the number of connected clients
int getConnectedClientsCount() {
    return connectedClientsCount.load();
}

void sendCommandToClients(const std::string& command) {
    std::lock_guard<std::mutex> lock(clientSocketsMutex);

    for (SOCKET clientSocket : clientSockets) {
        if (clientSocket != INVALID_SOCKET) {
            int result = send(clientSocket, command.c_str(), (int)command.size(), 0);
            if (result == SOCKET_ERROR) {
                std::cerr << "Failed to send command to a client: " << WSAGetLastError() << std::endl;
            }
        }
    }
}

