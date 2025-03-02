#include "server/server.h"
#include "application/app_launcher.h"
#include "dotenv.h"
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>

int main(int argc, char** argv) {
    try {
        // Load environment variables from .env file
        dotenv::init();
        
        // Get PORT from environment or use default (2354)
        int port = 2354;  // Default port
        
        // Try to get PORT from environment
        auto port_it = dotenv::env.find("PORT");
        if (port_it != dotenv::env.end()) {
            try {
                port = std::stoi(port_it->second);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid PORT value in .env file, using default: " << port << std::endl;
            }
        }
        
        // Create a server instance with the port from environment
        Server server(port);

        // Register some sample applications
        ApplicationLauncher::registerApplication({
            "chrome_google", 
            "Google Chrome", 
            "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe", 
            ApplicationLauncher::ApplicationType::EXECUTABLE,
            {"--profile-last-used", "google.com"}
        });

        ApplicationLauncher::registerApplication({
            "notepad", 
            "Notepad", 
            "C:\\Windows\\System32\\notepad.exe", 
            ApplicationLauncher::ApplicationType::EXECUTABLE,
            {}
        });

        // Configure message handler to support application launching
        server.setMessageHandler([](const nlohmann::json& message) {
            nlohmann::json response;
            
            try {
                std::string type = message.value("type", "");
                
                if (type == "launch_app") {
                    bool launched = false;
                    
                    // Check if a custom path is provided
                    if (message["data"].contains("path")) {
                        std::string path = message["data"]["path"];
                        
                        // Get arguments if provided, otherwise use empty vector
                        std::vector<std::string> arguments;
                        if (message["data"].contains("arguments")) {
                            arguments = message["data"]["arguments"].get<std::vector<std::string>>();
                        }
                        
                        // Launch with custom path and arguments
                        launched = ApplicationLauncher::launchApplication(path, arguments);
                        
                        response["type"] = "launch_result";
                        response["success"] = launched;
                        response["path"] = path;
                    } 
                    // Existing ID-based launch remains the same
                    else if (message["data"].contains("id")) {
                        std::string appId = message["data"]["id"];
                        launched = ApplicationLauncher::launchApplication(appId);
                        
                        response["type"] = "launch_result";
                        response["success"] = launched;
                        response["app_id"] = appId;
                    }
                    else {
                        response["type"] = "error";
                        response["message"] = "No path or ID provided";
                    }
                }
                else if (type == "list_apps") {
                    auto apps = ApplicationLauncher::getRegisteredApplications();
                    
                    response["type"] = "app_list";
                    response["apps"] = nlohmann::json::array();
                    
                    for (const auto& app : apps) {
                        nlohmann::json appJson = {
                            {"id", app.id},
                            {"name", app.name},
                            {"path", app.path}
                        };
                        
                        // Add icon data if available
                        if (app.icon.has_value()) {
                            appJson["icon"] = {
                                {"data", app.icon->base64Data},
                                {"mimeType", app.icon->mimeType}
                            };
                        }
                        
                        response["apps"].push_back(appJson);
                    }
                }
                else {
                    response["type"] = "error";
                    response["message"] = "Unknown message type";
                }
            } catch (const std::exception& e) {
                response["type"] = "error";
                response["message"] = e.what();
            }
            
            return response;
        });

        std::cout << "Starting server on port " << port << "..." << std::endl;

        // Start server directly in main thread for better error handling
        auto [success, errorMsg] = server.run();
        
        if (!success) {
            std::cerr << "Server failed to start: " << errorMsg << std::endl;
            return 1;
        }

        std::cout << "Server started successfully on port " << server.getPort() << std::endl;
        std::cout << "Press Ctrl+C to stop the server..." << std::endl;

        // Keep the main thread alive with a simple loop
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}