#pragma once
#include <string>
#include <vector>
#include <utility>
#include <windows.h> // For DWORD and DWORD_PTR

class Client {
public:
    Client(const std::string& serverAddress, int port);
    void Connect();
    void Reconnect();
    void ScanMemory();
    void SelectAddress(int index);
    void ModifyValue(int index, DWORD newValue);
    void Run();

private:
    std::string serverAddress;
    int port;
    SOCKET clientSocket;
    std::vector<std::pair<DWORD_PTR, DWORD>> memoryAddresses;

    void HandleServerCommands(const std::string& command);
    void DisplayAddresses();
};