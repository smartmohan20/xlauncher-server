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
        int port = 2354;                  // Default port
        std::string host = "127.0.0.1";   // Default host
        
        // Try to get PORT and HOST from environment
        auto port_it = dotenv::env.find("PORT");
        auto host_it = dotenv::env.find("HOST");

        if (port_it != dotenv::env.end()) {
            try {
                port = std::stoi(port_it->second);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Invalid PORT value in .env file, using default: " << port << std::endl;
            }
        }
       
        if (host_it != dotenv::env.end()) {
            host = host_it->second;
        } else {
            std::cout << "Warning: Invalid HOST value in .env file,, using default: " << host << std::endl;
        }
        
        // Create a server instance with the port from environment
        Server server(port, host);

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
                else if (type == "add_app") {
                    // Handle add application request
                    if (!message["data"].contains("path") || !message["data"]["path"].is_string()) {
                        response["type"] = "error";
                        response["message"] = "Missing or invalid path field";
                        return response;
                    }
                    
                    std::string path = message["data"]["path"];
                    std::string name;
                    std::string appId;
                    std::vector<std::string> arguments;
                    
                    // Generate a unique ID based on path if not provided
                    if (message["data"].contains("id") && message["data"]["id"].is_string()) {
                        appId = message["data"]["id"];
                    } else {
                        // Generate a unique ID from path - use a simple hash
                        size_t hashValue = std::hash<std::string>{}(path);
                        appId = "app_" + std::to_string(hashValue);
                    }
                    
                    // Get app name from data or extract from path
                    if (message["data"].contains("name") && message["data"]["name"].is_string()) {
                        name = message["data"]["name"];
                    } else {
                        // Extract filename from path as name
                        size_t lastSlash = path.find_last_of("/\\");
                        if (lastSlash != std::string::npos) {
                            name = path.substr(lastSlash + 1);
                        } else {
                            name = path;
                        }
                        
                        // Remove extension if present
                        size_t lastDot = name.find_last_of(".");
                        if (lastDot != std::string::npos) {
                            name = name.substr(0, lastDot);
                        }
                    }
                    
                    // Get application type
                    ApplicationLauncher::ApplicationType appType = ApplicationLauncher::ApplicationType::EXECUTABLE;
                    if (message["data"].contains("type") && message["data"]["type"].is_string()) {
                        std::string typeStr = message["data"]["type"];
                        appType = ApplicationLauncher::stringToApplicationType(typeStr);
                    } else {
                        // Auto-detect type based on path
                        if (path.find("http://") == 0 || path.find("https://") == 0) {
                            appType = ApplicationLauncher::ApplicationType::WEBSITE;
                        }
                    }
                    
                    // Get arguments if provided
                    if (message["data"].contains("arguments") && message["data"]["arguments"].is_array()) {
                        try {
                            arguments = message["data"]["arguments"].get<std::vector<std::string>>();
                        } catch (...) {
                            arguments.clear();
                        }
                    }
                    
                    // Create and register the application
                    ApplicationLauncher::Application newApp;
                    newApp.id = appId;
                    newApp.name = name;
                    newApp.path = path;
                    newApp.type = appType;
                    newApp.arguments = arguments;
                    
                    bool registered = false;
                    try {
                        registered = ApplicationLauncher::registerApplication(newApp);
                    } catch (const std::exception& e) {
                        response["type"] = "add_app_result";
                        response["success"] = false;
                        response["error"] = e.what();
                        return response;
                    }
                    
                    response["type"] = "add_app_result";
                    response["success"] = registered;
                    if (registered) {
                        response["app_id"] = appId;
                        
                        // Save the updated app list to config file
                        std::string configPath = "./config/apps.json";
                        // Try to get config path from environment
                        auto configPathIt = dotenv::env.find("APP_CONFIG_PATH");
                        if (configPathIt != dotenv::env.end() && !configPathIt->second.empty()) {
                            configPath = configPathIt->second;
                        }
                        
                        // Ensure directory exists
                        std::string directory = configPath.substr(0, configPath.find_last_of("/\\"));
                        if (!directory.empty()) {
                            CreateDirectoryA(directory.c_str(), NULL);
                        }
                        
                        ApplicationLauncher::saveApplicationsToFile(configPath);
                    } else {
                        response["error"] = "Failed to register application. ID may already exist.";
                    }
                }
                else if (type == "remove_app") {
                    // Handle remove application request
                    if (!message["data"].contains("id") || !message["data"]["id"].is_string()) {
                        response["type"] = "error";
                        response["message"] = "Missing or invalid app ID";
                        return response;
                    }
                    
                    std::string appId = message["data"]["id"];
                    bool removed = false;
                    
                    try {
                        removed = ApplicationLauncher::unregisterApplication(appId);
                    } catch (const std::exception& e) {
                        response["type"] = "remove_app_result";
                        response["success"] = false;
                        response["error"] = e.what();
                        return response;
                    }
                    
                    response["type"] = "remove_app_result";
                    response["success"] = removed;
                    response["app_id"] = appId;
                    
                    if (removed) {
                        // Save the updated app list to config file
                        std::string configPath = "./config/apps.json";
                        // Try to get config path from environment
                        auto configPathIt = dotenv::env.find("APP_CONFIG_PATH");
                        if (configPathIt != dotenv::env.end() && !configPathIt->second.empty()) {
                            configPath = configPathIt->second;
                        }
                        
                        ApplicationLauncher::saveApplicationsToFile(configPath);
                    } else {
                        response["error"] = "Application not found";
                    }
                }
                else if (type == "save_config") {
                    // Handle save configuration request
                    std::string configPath = "./config/apps.json";
                    
                    // Check if custom path is provided
                    if (message["data"].contains("path") && message["data"]["path"].is_string()) {
                        configPath = message["data"]["path"];
                    } else {
                        // Try to get config path from environment
                        auto configPathIt = dotenv::env.find("APP_CONFIG_PATH");
                        if (configPathIt != dotenv::env.end() && !configPathIt->second.empty()) {
                            configPath = configPathIt->second;
                        }
                    }
                    
                    // Ensure directory exists
                    std::string directory = configPath.substr(0, configPath.find_last_of("/\\"));
                    if (!directory.empty()) {
                        CreateDirectoryA(directory.c_str(), NULL);
                    }
                    
                    bool saved = false;
                    try {
                        saved = ApplicationLauncher::saveApplicationsToFile(configPath);
                    } catch (const std::exception& e) {
                        response["type"] = "save_config_result";
                        response["success"] = false;
                        response["error"] = e.what();
                        return response;
                    }
                    
                    response["type"] = "save_config_result";
                    response["success"] = saved;
                    response["config_path"] = configPath;
                    
                    if (!saved) {
                        response["error"] = "Failed to save configuration file";
                    }
                }
                else if (type == "load_config") {
                    // Handle load configuration request
                    std::string configPath = "./config/apps.json";
                    
                    // Check if custom path is provided
                    if (message["data"].contains("path") && message["data"]["path"].is_string()) {
                        configPath = message["data"]["path"];
                    } else {
                        // Try to get config path from environment
                        auto configPathIt = dotenv::env.find("APP_CONFIG_PATH");
                        if (configPathIt != dotenv::env.end() && !configPathIt->second.empty()) {
                            configPath = configPathIt->second;
                        }
                    }
                    
                    bool loaded = false;
                    try {
                        loaded = ApplicationLauncher::loadApplicationsFromFile(configPath);
                    } catch (const std::exception& e) {
                        response["type"] = "load_config_result";
                        response["success"] = false;
                        response["error"] = e.what();
                        return response;
                    }
                    
                    response["type"] = "load_config_result";
                    response["success"] = loaded;
                    response["config_path"] = configPath;
                    
                    if (!loaded) {
                        response["error"] = "Failed to load configuration file";
                    }
                }
                else if (type == "upload_config") {
                    // Handle config file upload
                    if (!message["data"].contains("content") || !message["data"]["content"].is_string()) {
                        response["type"] = "error";
                        response["message"] = "Missing or invalid config content";
                        return response;
                    }
                    
                    std::string configContent = message["data"]["content"];
                    std::string configPath = "./config/apps.json";
                    
                    // Check if custom path is provided
                    if (message["data"].contains("path") && message["data"]["path"].is_string()) {
                        configPath = message["data"]["path"];
                    } else {
                        // Try to get config path from environment
                        auto configPathIt = dotenv::env.find("APP_CONFIG_PATH");
                        if (configPathIt != dotenv::env.end() && !configPathIt->second.empty()) {
                            configPath = configPathIt->second;
                        }
                    }
                    
                    // Ensure directory exists
                    std::string directory = configPath.substr(0, configPath.find_last_of("/\\"));
                    if (!directory.empty()) {
                        CreateDirectoryA(directory.c_str(), NULL);
                    }
                    
                    // Write content to file
                    std::ofstream outFile(configPath);
                    if (!outFile.is_open()) {
                        response["type"] = "upload_config_result";
                        response["success"] = false;
                        response["error"] = "Failed to open config file for writing";
                        return response;
                    }
                    
                    outFile << configContent;
                    outFile.close();
                    
                    // Try to load the uploaded config
                    bool loaded = false;
                    try {
                        loaded = ApplicationLauncher::loadApplicationsFromFile(configPath);
                    } catch (const std::exception& e) {
                        response["type"] = "upload_config_result";
                        response["success"] = false;
                        response["error"] = std::string("File saved but failed to load: ") + e.what();
                        return response;
                    }
                    
                    response["type"] = "upload_config_result";
                    response["success"] = loaded;
                    response["config_path"] = configPath;
                    
                    if (!loaded) {
                        response["error"] = "File saved but failed to load applications";
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

        // Create config directory if it doesn't exist
        CreateDirectoryA("./config", NULL);

        // Load applications from config file if available
        std::string configPath = "./config/apps.json";
        // Try to get config path from environment
        auto configPathIt = dotenv::env.find("APP_CONFIG_PATH");
        if (configPathIt != dotenv::env.end() && !configPathIt->second.empty()) {
            configPath = configPathIt->second;
        }

        // Try to load applications, but don't fail if file doesn't exist
        try {
            ApplicationLauncher::loadApplicationsFromFile(configPath);
        } catch (...) {
            std::cout << "No existing configuration found, starting with default applications" << std::endl;
        }

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