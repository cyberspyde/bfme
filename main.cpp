#include "client.h"
#include <iostream>

int main() {
    std::string serverAddress = "26.242.87.42"; // Replace with actual server address
    int port = 8080; // Replace with actual port

    // Create a Client instance
    Client client(serverAddress, port);

    // Connect to the server and run the client
    client.Run();

    return 0;
}