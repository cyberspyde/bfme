#include <string>

#ifndef CONNECT_H
#define CONNECT_H


#include <windows.h>

// Function to handle the "Reconnect" button functionality
void issueReconnect();
void sendCommandToClients(const std::string& command);
// Function to get the number of connected clients
int getConnectedClientsCount();

#endif // CONNECT_H