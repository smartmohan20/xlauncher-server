#pragma once
#include "websocket_server.h"
#include "../screen_sharing.h"
#include <utility>
#include <string>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

class Server {
public:
    explicit Server(int port, const std::string& host = "127.0.0.1");
    ~Server();
    
    std::pair<bool, std::string> run();
    
    int getPort() const;

    void setMessageHandler(std::function<nlohmann::json(const nlohmann::json&)> handler);

private:
    SimpleSocketServer _socketServer;
    std::function<nlohmann::json(const nlohmann::json&)> _messageHandler;
    std::unique_ptr<ScreenSharing> _screenSharing;
    
    void initialize();
    void handleBinaryMessage(SOCKET client, const std::vector<uint8_t>& data);
};