#include "server.h"
#include <iostream>
#include "../screen_capture/screen_capture.h"
#include <turbojpeg.h>

Server::Server(int port, const std::string& host) : _socketServer(port, host) {
    initialize();
}

Server::~Server() {
    _socketServer.stop();
}

void Server::initialize() {
    std::cout << "Initializing server components..." << std::endl;
    
    // Create screen sharing instance
    _screenSharing = std::make_unique<ScreenSharing>();
    
    // Initialize screen sharing
    if (!_screenSharing->initialize()) {
        std::cerr << "Failed to initialize screen sharing" << std::endl;
    }
    
    // Set up the frame callback
    _screenSharing->setFrameCallback([this](const std::vector<uint8_t>& jpegData, int width, int height) {
        // Broadcast the frame to all clients
        _socketServer.broadcastBinaryMessage(jpegData);
    });
    
    // Set up binary message handler for the WebSocket server
    _socketServer.setBinaryMessageHandler([this](SOCKET client, const std::vector<uint8_t>& data) {
        handleBinaryMessage(client, data);
    });
    
    // Set up the message handler for the WebSocket server
    _socketServer.setMessageHandler([this](const std::string& message) {
        try {
            auto jsonMessage = nlohmann::json::parse(message);
            
            // Check if it's a screen sharing related message
            if (jsonMessage.contains("type")) {
                std::string messageType = jsonMessage["type"];
                
                if (messageType.find("screen_") == 0 || 
                    messageType == "start_sharing" || 
                    messageType == "stop_sharing" ||
                    messageType == "input_event") {
                    
                    // Handle screen sharing message
                    auto response = _screenSharing->handleMessage(jsonMessage);
                    return response.dump();
                }
            }
            
            // If not handled by screen sharing, use the regular message handler
            if (_messageHandler) {
                auto response = _messageHandler(jsonMessage);
                return response.dump();
            }
            
            std::cout << "Received message: " << message << std::endl;
            return std::string();
        } catch (const std::exception& e) {
            std::cerr << "Error processing message: " << e.what() << std::endl;
            
            // Return error response
            nlohmann::json errorResponse;
            errorResponse["type"] = "error";
            errorResponse["message"] = std::string("Error processing message: ") + e.what();
            return errorResponse.dump();
        }
    });
}

void Server::handleBinaryMessage(SOCKET client, const std::vector<uint8_t>& data) {
    // For now, we don't expect any binary messages from clients
    std::cout << "Received binary message of size: " << data.size() << " bytes" << std::endl;
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