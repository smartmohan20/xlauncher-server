#pragma once
#include "websocket_server.h"
#include <utility>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

class Server {
public:
    explicit Server(int port);
    ~Server();
    
    std::pair<bool, std::string> run();
    
    int getPort() const;

    void setMessageHandler(std::function<nlohmann::json(const nlohmann::json&)> handler);

private:
    SimpleSocketServer _socketServer;
    std::function<nlohmann::json(const nlohmann::json&)> _messageHandler;

    void initialize();
};