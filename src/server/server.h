#pragma once
#include "websocket_server.h"
#include <utility>
#include <string>

// Main server class that manages socket server and application logic
class Server {
public:
    explicit Server(int port);
    
    // Return type remains the same
    std::pair<bool, std::string> run();
    
    int getPort() const;

private:
    SimpleSocketServer _socketServer;

    void initialize();
};