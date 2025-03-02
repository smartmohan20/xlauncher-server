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
                // First validate that we have a proper JSON message
                if (!message.is_object()) {
                    response["type"] = "error";
                    response["message"] = "Invalid message format: expected JSON object";
                    return response;
                }
                
                // Extract message type with validation
                std::string type;
                try {
                    if (!message.contains("type")) {
                        response["type"] = "error";
                        response["message"] = "Missing required field: type";
                        return response;
                    }
                    
                    if (!message["type"].is_string()) {
                        response["type"] = "error";
                        response["message"] = "Invalid type field: expected string";
                        return response;
                    }
                    
                    type = message["type"];
                } catch (...) {
                    response["type"] = "error";
                    response["message"] = "Error processing message type";
                    return response;
                }
                
                // Validate data field exists for commands that need it
                if ((type == "launch_app" || type == "close_app") && 
                    (!message.contains("data") || !message["data"].is_object())) {
                    response["type"] = "error";
                    response["message"] = "Missing or invalid data field";
                    return response;
                }
                
                if (type == "launch_app") {
                    bool launched = false;
                    
                    // Check if a custom path is provided
                    if (message["data"].contains("path") && message["data"]["path"].is_string()) {
                        std::string path = message["data"]["path"];
                        
                        // Get arguments if provided, otherwise use empty vector
                        std::vector<std::string> arguments;
                        if (message["data"].contains("arguments") && message["data"]["arguments"].is_array()) {
                            try {
                                arguments = message["data"]["arguments"].get<std::vector<std::string>>();
                            } catch (...) {
                                // If conversion fails, use empty vector
                                arguments.clear();
                            }
                        }
                        
                        // Launch with custom path and arguments
                        try {
                            launched = ApplicationLauncher::launchApplication(path, arguments);
                        } catch (const std::exception& e) {
                            response["type"] = "launch_result";
                            response["success"] = false;
                            response["path"] = path;
                            response["error"] = e.what();
                            return response;
                        } catch (...) {
                            response["type"] = "launch_result";
                            response["success"] = false;
                            response["path"] = path;
                            response["error"] = "Unknown error during launch";
                            return response;
                        }
                        
                        response["type"] = "launch_result";
                        response["success"] = launched;
                        response["path"] = path;
                    } 
                    // Existing ID-based launch remains the same
                    else if (message["data"].contains("id") && message["data"]["id"].is_string()) {
                        std::string appId = message["data"]["id"];
                        
                        try {
                            launched = ApplicationLauncher::launchApplication(appId);
                        } catch (const std::exception& e) {
                            response["type"] = "launch_result";
                            response["success"] = false;
                            response["app_id"] = appId;
                            response["error"] = e.what();
                            return response;
                        } catch (...) {
                            response["type"] = "launch_result";
                            response["success"] = false;
                            response["app_id"] = appId;
                            response["error"] = "Unknown error during launch";
                            return response;
                        }
                        
                        response["type"] = "launch_result";
                        response["success"] = launched;
                        response["app_id"] = appId;
                    }
                    else {
                        response["type"] = "error";
                        response["message"] = "No path or ID provided";
                    }
                }
                else if (type == "close_app") {
                    // Handle close app request
                    if (message["data"].contains("id") && message["data"]["id"].is_string()) {
                        std::string appId = message["data"]["id"];
                        
                        bool closed = false;
                        try {
                            closed = ApplicationLauncher::closeApplication(appId);
                        } catch (const std::exception& e) {
                            response["type"] = "close_result";
                            response["success"] = false;
                            response["app_id"] = appId;
                            response["error"] = e.what();
                            return response;
                        } catch (...) {
                            response["type"] = "close_result";
                            response["success"] = false;
                            response["app_id"] = appId;
                            response["error"] = "Unknown error during close operation";
                            return response;
                        }
                        
                        response["type"] = "close_result";
                        response["success"] = closed;
                        response["app_id"] = appId;
                    }
                    else {
                        response["type"] = "error";
                        response["message"] = "No app ID provided";
                    }
                }
                else if (type == "list_apps") {
                    std::vector<ApplicationLauncher::Application> apps;
                    
                    try {
                        apps = ApplicationLauncher::getRegisteredApplications();
                    } catch (const std::exception& e) {
                        response["type"] = "error";
                        response["message"] = std::string("Error retrieving application list: ") + e.what();
                        return response;
                    } catch (...) {
                        response["type"] = "error";
                        response["message"] = "Unknown error retrieving application list";
                        return response;
                    }
                    
                    response["type"] = "app_list";
                    response["apps"] = nlohmann::json::array();
                    
                    for (const auto& app : apps) {
                        try {
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
                        } catch (...) {
                            // Skip this app if there's an error, but continue with others
                            continue;
                        }
                    }
                }
                else {
                    response["type"] = "error";
                    response["message"] = "Unknown message type: " + type;
                }
            } catch (const nlohmann::json::exception& e) {
                response["type"] = "error";
                response["message"] = std::string("JSON parsing error: ") + e.what();
            } catch (const std::exception& e) {
                response["type"] = "error";
                response["message"] = std::string("Error processing message: ") + e.what();
            } catch (...) {
                response["type"] = "error";
                response["message"] = "Unknown error occurred while processing message";
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