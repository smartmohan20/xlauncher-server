#pragma once

#define WINVER 0x0601  // Windows 7 or later
#define _WIN32_WINNT 0x0601

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <Windows.h>
#include <TlHelp32.h>  // For process enumeration

class ApplicationLauncher {
public:
    enum class ApplicationType {
        EXECUTABLE,
        WEBSITE,
        SYSTEM_COMMAND
    };

    struct IconData {
        std::string base64Data;
        std::string mimeType;
    };

    struct Application {
        std::string id;
        std::string name;
        std::string path;
        ApplicationType type;
        std::vector<std::string> arguments;
        std::optional<IconData> icon;
    };

    // Launch an application by its ID
    static bool launchApplication(const std::string& appId);

    // New method to launch with custom path and arguments
    static bool launchApplication(
        const std::string& path, 
        const std::vector<std::string>& arguments
    );

    // New method to close an application by its ID
    static bool closeApplication(const std::string& appId);

    // Register a new application
    static bool registerApplication(const Application& app);

    // Get list of registered applications
    static std::vector<Application> getRegisteredApplications();

    // Find application by ID
    static std::optional<Application> findApplicationById(const std::string& appId);

    // Extract icon from executable file
    static std::optional<IconData> extractIconFromPath(const std::string& path);

    // Save applications to config file
    static bool saveApplicationsToFile(const std::string& filePath);

    // Load applications from config file
    static bool loadApplicationsFromFile(const std::string& filePath);

    // Convert ApplicationType to string
    static std::string applicationTypeToString(ApplicationType type);

    // Convert string to ApplicationType
    static ApplicationType stringToApplicationType(const std::string& typeStr);

    // Unregister an application
    static bool unregisterApplication(const std::string& appId);

private:
    // Internal method to launch different types of applications
    static bool launchExecutable(const Application& app);
    static bool launchWebsite(const Application& app);
    static bool launchSystemCommand(const Application& app);

    // Store registered applications
    static std::vector<Application> _registeredApplications;
    
    // Store process handles for active applications (appId -> HANDLE)
    static std::map<std::string, HANDLE> _applicationProcesses;
};