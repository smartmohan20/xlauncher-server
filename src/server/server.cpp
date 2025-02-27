#include "server.h"
#include <iostream>

Server::Server(int port) : _socketServer(port) {
    initialize();
}

Server::~Server() {
    _socketServer.stop();
}

void Server::initialize() {
    std::cout << "Initializing server components..." << std::endl;
    
    _socketServer.setMessageHandler([this](const std::string& message) {
        try {
            auto jsonMessage = nlohmann::json::parse(message);
            
            if (_messageHandler) {
                auto response = _messageHandler(jsonMessage);
                return response.dump();
            }
            
            std::cout << "Received message: " << message << std::endl;
            return std::string();
        } catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
            return std::string();
        }
    });
}

std::pair<bool, std::string> Server::run() {
    std::cout << "Starting server on port " << _socketServer.getPort() << "..." << std::endl;
    return _socketServer.start();
}

int Server::getPort() const {
    return _socketServer.getPort();
}

void Server::setMessageHandler(std::function<nlohmann::json(const nlohmann::json&)> handler) {
    _messageHandler = std::move(handler);
}