#include "server.h"
#include <iostream>

// Constructor to initialize the server with a port
Server::Server(int port) : _socketServer(port) {
    initialize();
}

// Initialize server components
void Server::initialize() {
    std::cout << "Initializing server components..." << std::endl;
    
    // Set up message handler
    _socketServer.setMessageHandler([this](const std::string& message) {
        // Handle received messages here
        std::cout << "Received message: " << message << std::endl;
    });
}

// Main server loop
std::pair<bool, std::string> Server::run() {
    std::cout << "Starting server on port " << _socketServer.getPort() << "..." << std::endl;
    
    // Start the socket server
    return _socketServer.start();
}

// Get server port
int Server::getPort() const {
    return _socketServer.getPort();
}